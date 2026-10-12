"""Free-threading tests.

Every scenario runs in its own interpreter: a crash or a double free in the native
code fails one test instead of killing the pytest process, and x11 and egl contexts
do not get mixed in one process. The scenarios are plain functions in this file,
run by ``python free_threading_test.py <scenario>``.
"""
import ctypes
import importlib.util
import os
import platform
import subprocess
import sys
import sysconfig
import threading
from unittest import TestCase

import pytest

SKIP = 77  # exit code of a scenario that cannot run on this machine

FREE_THREADED = bool(sysconfig.get_config_var('Py_GIL_DISABLED'))

if platform.system() == 'Windows':
    NATIVE_MODULES = ['wgl', 'windowed', 'empty']
elif platform.system() == 'Darwin':
    NATIVE_MODULES = ['darwin', 'windowed', 'empty']
else:
    NATIVE_MODULES = ['egl', 'x11', 'headless', 'windowed', 'empty']

# Not GL_RENDERER: Mesa returns NULL for it intermittently when several threads query it at once,
# with plain ctypes calls too, that is unrelated to glcontext.
GL_VERSION = 0x1F02


# Scenarios, these run in the subprocess

def skip(reason):
    print('SKIP: %s' % reason)
    sys.exit(SKIP)


def run_threads(count, target):
    """Run target(index) in count threads released together, raise if any of them failed."""
    barrier = threading.Barrier(count)
    errors = []

    def wrapper(index):
        try:
            barrier.wait(10)
            target(index)
        except BaseException as e:
            errors.append(e)

    threads = [threading.Thread(target=wrapper, args=(i,)) for i in range(count)]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join(30)
        assert not thread.is_alive(), 'thread is stuck'
    if errors:
        raise errors[0]


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


def gl_string(ctx, name):
    glGetString = ctypes.cast(ctx.load('glGetString'), ctypes.CFUNCTYPE(ctypes.c_char_p, ctypes.c_uint32))
    return glGetString(name)


def scenario_create(name):
    """Every thread creates, uses and releases its own standalone contexts."""
    backend = backend_for(name)

    def work(index):
        for _ in range(3):
            ctx = backend(mode='standalone', glversion=330)
            assert ctx.standalone
            glGetError = ctypes.cast(ctx.load('glGetError'), ctypes.CFUNCTYPE(ctypes.c_uint32))
            glGetError()
            assert gl_string(ctx, GL_VERSION)
            with ctx:
                assert gl_string(ctx, GL_VERSION)
                assert glGetError() == 0
            ctx.release()

    run_threads(6, work)


def scenario_release_twice(name):
    """A second release() is a no-op, it used to destroy the native handles twice."""
    backend = backend_for(name)
    ctx = backend(mode='standalone', glversion=330)
    with ctx:
        ctx.release()  # __exit__ runs on a released context
    ctx = backend(mode='standalone', glversion=330)
    with ctx:
        pass
    ctx.release()
    ctx.release()
    with ctx:
        pass
    assert ctx.load('glGetString') > 0


def scenario_shared_release(name):
    """Threads call release() and everything else on one shared context at once."""
    backend = backend_for(name)
    # Making a context current in two threads is an X error that kills the process in GLX.
    concurrent_enter = name != 'x11'

    for _ in range(3):
        ctx = backend(mode='standalone', glversion=330)
        # Creating makes the context current here, releasing a context that is current on
        # another thread is a GL level error (libGL uses the closed display), so unbind it first.
        ctx.__exit__(None, None, None)

        def work(index):
            for i in range(50):
                if index % 2:
                    ctx.release()
                else:
                    if concurrent_enter:
                        with ctx:
                            pass
                    assert ctx.load('glGetString') > 0
                assert ctx.standalone

        run_threads(8, work)

        # Released for good: releasing again, entering and loading must not crash or double free.
        ctx.release()
        ctx.release()
        with ctx:
            pass
        assert ctx.load('glGetString') > 0


def scenario_headless():
    """The headless module keeps one global context, devices() and init() share global state."""
    try:
        from glcontext import headless
    except ImportError:
        skip('glcontext.headless is not built')

    try:
        devices = headless.devices()
        if not devices:
            skip('no EGL device')
        headless.init(device=0)
    except Exception as e:
        skip('headless is not available: %s' % e)

    def work(index):
        for _ in range(20):
            assert headless.devices() == devices
            assert headless.load_opengl_function('glEnable') > 0
            if index % 2:
                headless.init(device=0)

    run_threads(6, work)


def scenario_windowed():
    """windowed only has a stateless function to look up OpenGL functions."""
    try:
        from glcontext import windowed
    except ImportError:
        skip('glcontext.windowed is not built')

    expected = windowed.load_opengl_function('glEnable')

    def work(index):
        for _ in range(100):
            assert windowed.load_opengl_function('glEnable') == expected

    run_threads(6, work)


def scenario_empty():
    try:
        from glcontext import empty
    except ImportError:
        skip('glcontext.empty is not built')

    def work(index):
        for _ in range(100):
            ctx = empty.create_context()
            with ctx:
                assert ctx.load_opengl_function('glEnable') == 0
            ctx.release()

    run_threads(6, work)


SCENARIOS = {
    'egl_create': lambda: scenario_create('egl'),
    'egl_release_twice': lambda: scenario_release_twice('egl'),
    'egl_shared_release': lambda: scenario_shared_release('egl'),
    'x11_create': lambda: scenario_create('x11'),
    'x11_release_twice': lambda: scenario_release_twice('x11'),
    'x11_shared_release': lambda: scenario_shared_release('x11'),
    'headless': scenario_headless,
    'windowed': scenario_windowed,
    'empty': scenario_empty,
}


# Tests, these run in pytest

def run_python(args, env=None, timeout=60):
    return subprocess.run(
        [sys.executable] + args,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        universal_newlines=True,
        env=env,
        timeout=timeout,
    )


def run_scenario(name):
    result = run_python([os.path.abspath(__file__), name])
    if result.returncode == SKIP:
        pytest.skip(result.stdout.strip())
    assert result.returncode == 0, 'scenario %s exited with %s\n%s' % (name, result.returncode, result.stdout)


class FreeThreadingTestCase(TestCase):

    @pytest.mark.skipif(not FREE_THREADED, reason='requires a free-threaded build')
    def test_import_keeps_gil_disabled(self):
        """Importing the extension modules must not re-enable the GIL."""
        env = {k: v for k, v in os.environ.items() if k != 'PYTHON_GIL'}
        code = (
            'import sys, warnings, importlib\n'
            'warnings.simplefilter("error", RuntimeWarning)\n'
            'assert not sys._is_gil_enabled()\n'
            'importlib.import_module("glcontext.%s")\n'
            'assert not sys._is_gil_enabled(), "importing glcontext.%s enabled the GIL"\n'
        )
        tested = 0
        for name in NATIVE_MODULES:
            if importlib.util.find_spec('glcontext.' + name) is None:
                continue  # not every module is built by setup.py
            result = run_python(['-c', code % (name, name)], env=env)
            self.assertEqual(result.returncode, 0, 'glcontext.%s\n%s' % (name, result.stdout))
            tested += 1
        if not tested:
            self.skipTest('no extension module is built')

    def test_concurrent_egl_create(self):
        run_scenario('egl_create')

    def test_egl_release_twice(self):
        run_scenario('egl_release_twice')

    def test_concurrent_egl_shared_release(self):
        run_scenario('egl_shared_release')

    @pytest.mark.skipif(not sys.platform.startswith('linux'), reason='requires x11')
    def test_concurrent_x11_create(self):
        run_scenario('x11_create')

    @pytest.mark.skipif(not sys.platform.startswith('linux'), reason='requires x11')
    def test_x11_release_twice(self):
        run_scenario('x11_release_twice')

    @pytest.mark.skipif(not sys.platform.startswith('linux'), reason='requires x11')
    def test_concurrent_x11_shared_release(self):
        run_scenario('x11_shared_release')

    @pytest.mark.skipif(not sys.platform.startswith('linux'), reason='requires egl')
    def test_concurrent_headless(self):
        run_scenario('headless')

    def test_concurrent_windowed(self):
        run_scenario('windowed')

    def test_concurrent_empty(self):
        run_scenario('empty')


if __name__ == '__main__':
    SCENARIOS[sys.argv[1]]()
