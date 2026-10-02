"""Error path and cleanup tests.

Every scenario runs in its own interpreter: a crash in the native code fails one test
instead of killing the pytest process. The scenarios are plain functions in this file,
run by ``python error_paths_test.py <scenario>``.
"""
import ctypes
import gc
import os
import shutil
import subprocess
import sys
import tempfile
import threading
from unittest import TestCase

import pytest

SKIP = 77  # exit code of a scenario that cannot run on this machine

LINUX = sys.platform.startswith('linux')

# A stand-in for libGL, libX11 and libEGL, the modules load every function from the libraries
# they are given. It records what the modules call, so the cleanup and error paths can be checked
# without a GPU and without a display, including the failures that cannot be provoked with the real
# libraries. The current context is per thread like in GLX and EGL.
STUB_SOURCE = r"""
typedef void * ptr;

int n_glx_create, n_glx_destroy, n_glx_bind, n_glx_unbind, n_glx_unbind_other;
int n_x_open, n_x_close, n_x_destroy_window, n_x_free;
int n_egl_create, n_egl_destroy, n_egl_bind, n_egl_unbind;
int fail_choose_fbconfig, fail_choose_visual, fail_create_context, fail_no_arb;

static __thread ptr glx_current;
static __thread ptr glx_unbound;  // what the last glXMakeCurrent(NULL) of this thread unbound
static __thread ptr egl_current;
static ptr x_handler = (ptr)1;  // the Xlib default handler

void stub_set_glx_current(ptr ctx) { glx_current = ctx; }
ptr stub_get_glx_current(void) { return glx_current; }
void stub_set_egl_current(ptr ctx) { egl_current = ctx; }
ptr stub_get_egl_current(void) { return egl_current; }
ptr stub_get_x_handler(void) { return x_handler; }

struct visual_info { void * visual; unsigned long visualid; int screen; int depth; char pad[128]; };
static struct visual_info visual_info = {0, 1, 0, 24};
static ptr fbconfigs[2] = {(ptr)1, 0};

// libX11
ptr XOpenDisplay(const char * name) { return (ptr)(0x100000 + 0x1000 * ++n_x_open); }
int XDefaultScreen(ptr dpy) { return 0; }
unsigned long XRootWindow(ptr dpy, int screen) { return 1; }
unsigned long XCreateColormap(ptr dpy, unsigned long wnd, ptr visual, int alloc) { return 1; }
unsigned long XCreateWindow() { return 42; }
int XDestroyWindow(ptr dpy, unsigned long wnd) { n_x_destroy_window++; return 1; }
int XCloseDisplay(ptr dpy) { n_x_close++; return 0; }
int XFree(ptr data) { n_x_free++; return 1; }
ptr XSetErrorHandler(ptr handler) {
    ptr previous = x_handler;
    x_handler = handler ? handler : (ptr)1;
    return previous;
}

// libGL, GLX
ptr glXChooseFBConfig(ptr dpy, int screen, const int * attribs, int * n) { return fail_choose_fbconfig ? 0 : fbconfigs; }
ptr glXChooseVisual(ptr dpy, int screen, int * attribs) { return fail_choose_visual ? 0 : &visual_info; }
ptr glXGetCurrentDisplay(void) { return (ptr)0xd15; }
ptr glXGetCurrentContext(void) { return glx_current; }
unsigned long glXGetCurrentDrawable(void) { return 42; }
int glXMakeCurrent(ptr dpy, unsigned long drawable, ptr ctx) {
    ctx ? n_glx_bind++ : n_glx_unbind++;
    glx_unbound = ctx ? 0 : glx_current;
    glx_current = ctx;
    return 1;
}
void glXDestroyContext(ptr dpy, ptr ctx) {
    // unbinding one context and destroying another one is what n_glx_unbind_other counts
    if (glx_unbound && glx_unbound != ctx) n_glx_unbind_other++;
    glx_unbound = 0;
    n_glx_destroy++;
}
ptr glXCreateContext(ptr dpy, ptr vi, ptr share, int direct) {
    return fail_create_context ? 0 : (ptr)(0x200000 + 0x10 * ++n_glx_create);
}
ptr glXCreateContextAttribsARB(ptr dpy, ptr fbc, ptr share, int direct, const int * attribs) {
    return glXCreateContext(dpy, 0, share, direct);
}
static int streq(const char * a, const char * b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}
ptr glXGetProcAddress(const unsigned char * name) {
    if (fail_no_arb) return 0;
    return streq((const char *)name, "glXCreateContextAttribsARB") ? (ptr)glXCreateContextAttribsARB : 0;
}

// libEGL
int eglGetError(void) { return 0x3000; }
ptr eglGetDisplay(ptr native) { return (ptr)1; }
unsigned eglInitialize(ptr dpy, int * major, int * minor) { return 1; }
unsigned eglChooseConfig(ptr dpy, const int * attribs, ptr * configs, int size, int * n) { configs[0] = (ptr)1; *n = 1; return 1; }
unsigned eglBindAPI(unsigned api) { return 1; }
ptr eglCreateContext(ptr dpy, ptr config, ptr share, const int * attribs) {
    return fail_create_context ? 0 : (ptr)(0x300000 + 0x10 * ++n_egl_create);
}
unsigned eglDestroyContext(ptr dpy, ptr ctx) { n_egl_destroy++; return 1; }
unsigned eglMakeCurrent(ptr dpy, ptr draw, ptr read, ptr ctx) {
    ctx ? n_egl_bind++ : n_egl_unbind++;
    egl_current = ctx;
    return 1;
}
unsigned eglQueryDevicesEXT(int max, ptr * devices, int * n) { if (devices) devices[0] = (ptr)1; *n = 1; return 1; }
ptr eglGetPlatformDisplayEXT(unsigned platform, ptr native, const int * attribs) { return (ptr)1; }
ptr eglGetCurrentDisplay(void) { return (ptr)1; }
ptr eglGetCurrentContext(void) { return egl_current; }
ptr eglGetCurrentSurface(int which) { return (ptr)1; }
ptr eglGetProcAddress(const char * name) {
    if (streq(name, "eglQueryDevicesEXT")) return (ptr)eglQueryDevicesEXT;
    if (streq(name, "eglGetPlatformDisplayEXT")) return (ptr)eglGetPlatformDisplayEXT;
    if (streq(name, "eglGetCurrentDisplay")) return (ptr)eglGetCurrentDisplay;
    if (streq(name, "eglGetCurrentContext")) return (ptr)eglGetCurrentContext;
    if (streq(name, "eglGetCurrentSurface")) return (ptr)eglGetCurrentSurface;
    return 0;
}
"""


# Scenarios, these run in the subprocess

def skip(reason):
    print('SKIP: %s' % reason)
    sys.exit(SKIP)


class Stub:
    """The compiled stub library, the counters are read and the switches are set as attributes."""

    def __init__(self):
        compiler = shutil.which('cc') or shutil.which('gcc')
        if not LINUX or not compiler:
            skip('needs a C compiler on Linux to build the stub library')
        self.tmp = tempfile.mkdtemp()
        source = os.path.join(self.tmp, 'stub.c')
        self.path = os.path.join(self.tmp, 'libstub.so')
        with open(source, 'w') as f:
            f.write(STUB_SOURCE)
        result = subprocess.run([compiler, '-shared', '-fPIC', '-o', self.path, source],
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
        if result.returncode:
            skip('cannot build the stub library: %s' % result.stdout)
        self.lib = ctypes.CDLL(self.path)
        for name in ('stub_get_glx_current', 'stub_get_egl_current', 'stub_get_x_handler', 'XSetErrorHandler'):
            getattr(self.lib, name).restype = ctypes.c_void_p
        self.lib.XSetErrorHandler.argtypes = [ctypes.c_void_p]
        for name in ('stub_set_glx_current', 'stub_set_egl_current'):
            getattr(self.lib, name).argtypes = [ctypes.c_void_p]

    def __getattr__(self, name):
        if name.startswith(('n_', 'fail_')):
            return ctypes.c_int.in_dll(self.lib, name).value
        raise AttributeError(name)

    def __setattr__(self, name, value):
        if name.startswith('fail_'):
            ctypes.c_int.in_dll(self.lib, name).value = value
        else:
            object.__setattr__(self, name, value)

    def x11_context(self, **kwargs):
        from glcontext import x11
        return x11.create_context(libgl=self.path, libx11=self.path, **kwargs)

    def egl_context(self, **kwargs):
        from glcontext import egl
        return egl.create_context(libgl=self.path, libegl=self.path, **kwargs)

    def cleanup(self):
        shutil.rmtree(self.tmp, ignore_errors=True)


def backend_for(name):
    import glcontext
    try:
        if name == 'egl':
            backend = glcontext.get_backend_by_name('egl')
        else:
            backend = glcontext.default_backend()
        backend(mode='standalone', glversion=330).release()
    except Exception as e:
        skip('%s backend is not available: %s' % (name, e))
    return backend


def scenario_load_type_error(name):
    """load() with something that is not a str raises, it used to look up a NULL name and crash."""
    backend = backend_for(name)
    ctx = backend(mode='standalone', glversion=330)
    for method in (ctx.load, ctx.load_opengl_function):
        for arg in (123, None, b'glEnable', 1.5, object()):
            try:
                method(arg)
            except TypeError:
                pass
            else:
                raise AssertionError('load(%r) did not raise TypeError' % (arg,))
        try:
            method('\ud800')  # not encodable as UTF-8
        except UnicodeEncodeError:
            pass
        else:
            raise AssertionError('load of a lone surrogate did not raise UnicodeEncodeError')
        assert method('glEnable') > 0  # still works after the errors
    ctx.release()


def type_refcount_stays(create):
    """Creating and dropping contexts must not leak references to the type, every instance holds one."""
    ctx = create()
    kind = type(ctx)
    ctx.release()
    del ctx
    gc.collect()
    before = sys.getrefcount(kind)
    for _ in range(50):
        create()
    gc.collect()
    assert sys.getrefcount(kind) == before, 'type refcount %d -> %d' % (before, sys.getrefcount(kind))


def scenario_x11_dealloc_releases():
    """Dropping a context that was never released destroys it, the window and the display."""
    stub = Stub()
    ctx = stub.x11_context(mode='standalone')
    assert stub.n_glx_create == 1 and stub.n_glx_destroy == 0
    assert stub.n_x_open == 1 and stub.n_x_close == 0
    assert stub.lib.stub_get_glx_current()  # created contexts are current
    del ctx
    assert stub.n_glx_destroy == 1, 'the context was not destroyed'
    assert stub.n_x_destroy_window == 1, 'the window was not destroyed'
    assert stub.n_x_close == 1, 'the display was not closed'
    assert stub.n_x_free == 2, 'fbconfig and visual were not freed'
    assert not stub.lib.stub_get_glx_current(), 'the dropped context is still current'

    # an explicit release() first leaves nothing to do
    ctx = stub.x11_context(mode='standalone')
    ctx.release()
    assert (stub.n_glx_destroy, stub.n_x_close) == (2, 2)
    del ctx
    assert (stub.n_glx_destroy, stub.n_x_close) == (2, 2), 'destroyed twice'
    stub.cleanup()


def scenario_x11_dealloc_unrelated_context():
    """Dropping a context must not unbind or destroy what it does not own."""
    stub = Stub()
    ctx = stub.x11_context(mode='standalone')
    stub.lib.stub_set_glx_current(0xbeef)  # an unrelated context is current on this thread
    unbind = stub.n_glx_unbind
    del ctx
    assert stub.n_glx_destroy == 1
    assert stub.lib.stub_get_glx_current() == 0xbeef, 'the unrelated context was unbound'
    assert stub.n_glx_unbind == unbind

    # dropped while the thread that created it, where it is current, is alive. With the GIL the
    # last reference is dropped here, a free-threaded build may hand the object back to its owner.
    created = threading.Event()
    finish = threading.Event()
    holder = []

    def creator():
        holder.append(stub.x11_context(mode='standalone'))
        created.set()
        finish.wait(30)

    thread = threading.Thread(target=creator)
    thread.start()
    assert created.wait(30)
    stub.lib.stub_set_glx_current(0xbeef)
    holder.clear()
    for _ in range(300):
        if stub.n_glx_destroy == 2:
            break
        threading.Event().wait(0.01)
    finish.set()
    thread.join()
    assert stub.n_glx_destroy == 2, 'the context was not destroyed'
    assert stub.n_glx_unbind_other == 0, 'unbound a context that is not the one being destroyed'
    assert stub.lib.stub_get_glx_current() == 0xbeef, 'the unrelated context was unbound'

    # a detected context belongs to the application
    stub.lib.stub_set_glx_current(0xbeef)
    ctx = stub.x11_context(mode='detect')
    assert not ctx.standalone
    del ctx
    assert stub.n_glx_destroy == 2 and stub.n_x_close == 2 and stub.lib.stub_get_glx_current() == 0xbeef

    # a shared context is destroyed, the display belongs to the application
    ctx = stub.x11_context(mode='share')
    del ctx
    assert stub.n_glx_destroy == 3 and stub.n_x_close == 2
    stub.cleanup()


def scenario_x11_type_refcount():
    stub = Stub()
    type_refcount_stays(lambda: stub.x11_context(mode='standalone'))
    stub.cleanup()


def scenario_egl_dealloc_releases():
    """Dropping a context that was never released destroys it."""
    stub = Stub()
    ctx = stub.egl_context(mode='standalone')
    assert stub.n_egl_create == 1 and stub.n_egl_destroy == 0
    assert stub.lib.stub_get_egl_current()
    del ctx
    assert stub.n_egl_destroy == 1, 'the context was not destroyed'
    assert not stub.lib.stub_get_egl_current(), 'a destroyed context stays current until something else is bound'

    ctx = stub.egl_context(mode='standalone')
    ctx.release()
    assert stub.n_egl_destroy == 2
    del ctx
    assert stub.n_egl_destroy == 2, 'destroyed twice'
    stub.cleanup()


def scenario_egl_dealloc_unrelated_context():
    stub = Stub()
    ctx = stub.egl_context(mode='standalone')
    stub.lib.stub_set_egl_current(0xbeef)  # an unrelated context is current on this thread
    unbind = stub.n_egl_unbind
    del ctx
    assert stub.n_egl_destroy == 1
    assert stub.lib.stub_get_egl_current() == 0xbeef, 'the unrelated context was unbound'
    assert stub.n_egl_unbind == unbind
    stub.cleanup()


def scenario_egl_type_refcount():
    stub = Stub()
    type_refcount_stays(lambda: stub.egl_context(mode='standalone'))
    stub.cleanup()


X11_DEFAULT_HANDLER = 1  # what the stub reports for the Xlib default error handler

# the ways creating an x11 context can fail after the silent error handler was installed
X11_FAILURES = [
    ('standalone', {'fail_create_context': 1}, {}),
    ('standalone', {'fail_no_arb': 1}, {}),
    ('standalone', {'fail_create_context': 1}, {'glversion': 0}),
    ('share', {'fail_create_context': 1}, {}),
    ('share', {'fail_no_arb': 1}, {}),
    ('share', {'fail_create_context': 1}, {'glversion': 0}),
]


def x11_failing_create(stub, mode, switches, kwargs):
    """Make creating an x11 context fail the given way, returns the error."""
    stub.lib.stub_set_glx_current(0xbeef)  # the share mode needs a current context
    for name, value in switches.items():
        setattr(stub, name, value)
    try:
        stub.x11_context(mode=mode, **kwargs)
    except Exception as e:
        return e
    finally:
        for name in switches:
            setattr(stub, name, 0)
    raise AssertionError('creating the context did not fail: %s %s %s' % (mode, switches, kwargs))


def scenario_x11_handler_restored_after_failure():
    """The silent X error handler used to stay installed when the context could not be created."""
    stub = Stub()
    for mode, switches, kwargs in X11_FAILURES:
        assert stub.lib.stub_get_x_handler() == X11_DEFAULT_HANDLER
        x11_failing_create(stub, mode, switches, kwargs)
        assert stub.lib.stub_get_x_handler() == X11_DEFAULT_HANDLER, \
            'the silent error handler is still installed after %s %s %s' % (mode, switches, kwargs)
    stub.cleanup()


def scenario_x11_handler_of_the_application_is_kept():
    """The handler was reset to the Xlib default, not to the one that was installed before."""
    stub = Stub()
    stub.lib.XSetErrorHandler(0x1234)  # the application installs its own handler
    for mode in ('standalone', 'share'):
        stub.lib.stub_set_glx_current(0xbeef)
        ctx = stub.x11_context(mode=mode)
        assert stub.lib.stub_get_x_handler() == 0x1234, 'the handler of the application is gone after creating'
        ctx.release()
    for mode, switches, kwargs in X11_FAILURES:
        x11_failing_create(stub, mode, switches, kwargs)
        assert stub.lib.stub_get_x_handler() == 0x1234, \
            'the handler of the application is gone after %s %s %s' % (mode, switches, kwargs)
    stub.cleanup()


def scenario_x11_handler_of_the_application_is_kept_real():
    """The same with libX11, creating a context must not take over the error handling of the application."""
    from ctypes.util import find_library
    if not os.environ.get('DISPLAY'):
        skip('no display')
    libx11 = ctypes.CDLL(find_library('X11'))
    libx11.XSetErrorHandler.restype = ctypes.c_void_p
    libx11.XSetErrorHandler.argtypes = [ctypes.c_void_p]
    backend = backend_for('x11')

    @ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p)
    def handler(display, event):
        return 0

    address = ctypes.cast(handler, ctypes.c_void_p).value
    libx11.XSetErrorHandler(address)
    ctx = backend(mode='standalone', glversion=330)
    assert libx11.XSetErrorHandler(address) == address, 'the handler is gone after creating a context'
    ctx.release()
    try:
        backend(mode='standalone', glversion=999)  # there is no OpenGL 9.9
    except Exception:
        assert libx11.XSetErrorHandler(address) == address, 'the handler is gone after a failed create'
    else:
        skip('creating an OpenGL 9.9 context did not fail')


def scenario_x11_handler_restored_after_failure_real():
    """The same with libX11, an application that sets its own error handler would crash on the next X error."""
    from ctypes.util import find_library
    if not os.environ.get('DISPLAY'):
        skip('no display')
    libx11 = ctypes.CDLL(find_library('X11'))
    libx11.XSetErrorHandler.restype = ctypes.c_void_p
    libx11.XSetErrorHandler.argtypes = [ctypes.c_void_p]
    backend = backend_for('x11')
    default = libx11.XSetErrorHandler(None)
    try:
        backend(mode='standalone', glversion=999)  # there is no OpenGL 9.9
    except Exception:
        pass
    else:
        skip('creating an OpenGL 9.9 context did not fail')
    assert libx11.XSetErrorHandler(None) == default, 'the silent error handler is still installed'


def open_fds():
    return len(os.listdir('/proc/self/fd'))


def rss_kb():
    with open('/proc/self/statm') as f:
        return int(f.read().split()[1]) * os.sysconf('SC_PAGE_SIZE') // 1024


def scenario_x11_dealloc_real():
    """Contexts that are dropped without release() add up to X connections, the X server stops at 256 clients."""
    backend = backend_for('x11')
    fds = open_fds()
    for _ in range(300):
        backend(mode='standalone', glversion=330)
    assert open_fds() < fds + 20, 'leaked %d file descriptors' % (open_fds() - fds)


def scenario_egl_dealloc_real():
    """Contexts that are dropped without release() keep their memory, about 2 MB each with Mesa."""
    backend = backend_for('egl')
    for _ in range(10):  # warm up
        backend(mode='standalone', glversion=330).release()
    before = rss_kb()
    for _ in range(100):
        backend(mode='standalone', glversion=330)
    assert rss_kb() < before + 50 * 1024, 'memory grew by %d MB' % ((rss_kb() - before) // 1024)


def scenario_exit_with_live_contexts(name):
    """Contexts that are still alive at shutdown are destroyed by the interpreter, that must not crash."""
    global live_contexts
    backend = backend_for(name)
    live_contexts = [backend(mode='standalone', glversion=330) for _ in range(3)]


SCENARIOS = {
    'egl_load_type_error': lambda: scenario_load_type_error('egl'),
    'x11_load_type_error': lambda: scenario_load_type_error('x11'),
    'x11_dealloc_releases': scenario_x11_dealloc_releases,
    'x11_dealloc_unrelated_context': scenario_x11_dealloc_unrelated_context,
    'x11_type_refcount': scenario_x11_type_refcount,
    'x11_dealloc_real': scenario_x11_dealloc_real,
    'x11_handler_restored_after_failure': scenario_x11_handler_restored_after_failure,
    'x11_handler_restored_after_failure_real': scenario_x11_handler_restored_after_failure_real,
    'x11_handler_of_the_application_is_kept': scenario_x11_handler_of_the_application_is_kept,
    'x11_handler_of_the_application_is_kept_real': scenario_x11_handler_of_the_application_is_kept_real,
    'egl_dealloc_releases': scenario_egl_dealloc_releases,
    'egl_dealloc_unrelated_context': scenario_egl_dealloc_unrelated_context,
    'egl_type_refcount': scenario_egl_type_refcount,
    'egl_dealloc_real': scenario_egl_dealloc_real,
    'x11_exit_with_live_contexts': lambda: scenario_exit_with_live_contexts('x11'),
    'egl_exit_with_live_contexts': lambda: scenario_exit_with_live_contexts('egl'),
}


# Tests, these run in pytest

def run_scenario(name):
    result = subprocess.run(
        [sys.executable, os.path.abspath(__file__), name],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        universal_newlines=True,
        timeout=60,
    )
    if result.returncode == SKIP:
        pytest.skip(result.stdout.strip())
    assert result.returncode == 0, 'scenario %s exited with %s\n%s' % (name, result.returncode, result.stdout)


class ErrorPathsTestCase(TestCase):

    @pytest.mark.skipif(not LINUX, reason='requires egl')
    def test_egl_load_type_error(self):
        run_scenario('egl_load_type_error')

    @pytest.mark.skipif(not LINUX, reason='requires x11')
    def test_x11_load_type_error(self):
        run_scenario('x11_load_type_error')

    @pytest.mark.skipif(not LINUX, reason='requires x11')
    def test_x11_dealloc_releases(self):
        run_scenario('x11_dealloc_releases')

    @pytest.mark.skipif(not LINUX, reason='requires x11')
    def test_x11_dealloc_unrelated_context(self):
        run_scenario('x11_dealloc_unrelated_context')

    @pytest.mark.skipif(not LINUX, reason='requires x11')
    def test_x11_type_refcount(self):
        run_scenario('x11_type_refcount')

    @pytest.mark.skipif(not LINUX, reason='requires x11')
    def test_x11_dealloc_real(self):
        run_scenario('x11_dealloc_real')

    @pytest.mark.skipif(not LINUX, reason='requires x11')
    def test_x11_handler_restored_after_failure(self):
        run_scenario('x11_handler_restored_after_failure')

    @pytest.mark.skipif(not LINUX, reason='requires x11')
    def test_x11_handler_restored_after_failure_real(self):
        run_scenario('x11_handler_restored_after_failure_real')

    @pytest.mark.skipif(not LINUX, reason='requires x11')
    def test_x11_handler_of_the_application_is_kept(self):
        run_scenario('x11_handler_of_the_application_is_kept')

    @pytest.mark.skipif(not LINUX, reason='requires x11')
    def test_x11_handler_of_the_application_is_kept_real(self):
        run_scenario('x11_handler_of_the_application_is_kept_real')

    @pytest.mark.skipif(not LINUX, reason='requires x11')
    def test_x11_exit_with_live_contexts(self):
        run_scenario('x11_exit_with_live_contexts')

    @pytest.mark.skipif(not LINUX, reason='requires egl')
    def test_egl_dealloc_releases(self):
        run_scenario('egl_dealloc_releases')

    @pytest.mark.skipif(not LINUX, reason='requires egl')
    def test_egl_dealloc_unrelated_context(self):
        run_scenario('egl_dealloc_unrelated_context')

    @pytest.mark.skipif(not LINUX, reason='requires egl')
    def test_egl_type_refcount(self):
        run_scenario('egl_type_refcount')

    @pytest.mark.skipif(not LINUX, reason='requires egl')
    def test_egl_dealloc_real(self):
        run_scenario('egl_dealloc_real')

    @pytest.mark.skipif(not LINUX, reason='requires egl')
    def test_egl_exit_with_live_contexts(self):
        run_scenario('egl_exit_with_live_contexts')


if __name__ == '__main__':
    SCENARIOS[sys.argv[1]]()
