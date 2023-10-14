#include <Python.h>
#include <structmember.h>

#include <dlfcn.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define GLX_CONTEXT_MAJOR_VERSION 0x2091
#define GLX_CONTEXT_MINOR_VERSION 0x2092
#define GLX_CONTEXT_PROFILE_MASK 0x9126
#define GLX_CONTEXT_CORE_PROFILE_BIT 0x0001

#define GLX_RGBA 4
#define GLX_DOUBLEBUFFER 5
#define GLX_RED_SIZE 8
#define GLX_GREEN_SIZE 9
#define GLX_BLUE_SIZE 10
#define GLX_DEPTH_SIZE 12

typedef struct __GLXcontextRec * GLXContext;
typedef struct __GLXFBConfigRec * GLXFBConfig;
typedef XID GLXDrawable;

typedef GLXFBConfig * (* m_glXChooseFBConfigProc)(Display *, int, const int *, int *);
typedef XVisualInfo * (* m_glXChooseVisualProc)(Display *, int, int *);
typedef Display * (* m_glXGetCurrentDisplayProc)();
typedef GLXContext (* m_glXGetCurrentContextProc)();
typedef GLXDrawable (* m_glXGetCurrentDrawableProc)();
typedef Bool (* m_glXMakeCurrentProc)(Display *, GLXDrawable, GLXContext);
typedef void (* m_glXDestroyContextProc)(Display *, GLXContext);
typedef GLXContext (* m_glXCreateContextProc)(Display *, XVisualInfo *, GLXContext, Bool);
typedef void (*(* m_glXGetProcAddressProc)(const unsigned char *))();
typedef GLXContext (* m_glXCreateContextAttribsARBProc)(Display *, GLXFBConfig, GLXContext, int, const int *);

typedef Display * (* m_XOpenDisplayProc)(const char *);
typedef int (* m_XDefaultScreenProc)(Display *);
typedef Window (* m_XRootWindowProc)(Display *, int);
typedef Colormap (* m_XCreateColormapProc)(Display *, Window, Visual *, int);
typedef Window (* m_XCreateWindowProc)(Display *, Window, int, int, unsigned int, unsigned int, unsigned int, int, unsigned int, Visual *, unsigned long, XSetWindowAttributes *);
typedef int (* m_XDestroyWindowProc)(Display *, Window);
typedef int (* m_XCloseDisplayProc)(Display *);
typedef int (* m_XFreeProc)(void *);
typedef XErrorHandler (* m_XSetErrorHandlerProc)(XErrorHandler);

int SilentXErrorHandler(Display * d, XErrorEvent * e) {
    return 0;
}

struct GLContext {
    PyObject_HEAD

    void * libgl;
    void * libx11;
    Display * dpy;
    GLXFBConfig * fbc;
    XVisualInfo * vi;
    Window wnd;
    GLXContext ctx;

    int standalone;
    int own_window;
    void * old_context;
    void * old_display;
    void * old_window;

    m_glXChooseFBConfigProc m_glXChooseFBConfig;
    m_glXChooseVisualProc m_glXChooseVisual;
    m_glXGetCurrentDisplayProc m_glXGetCurrentDisplay;
    m_glXGetCurrentContextProc m_glXGetCurrentContext;
    m_glXGetCurrentDrawableProc m_glXGetCurrentDrawable;
    m_glXMakeCurrentProc m_glXMakeCurrent;
    m_glXDestroyContextProc m_glXDestroyContext;
    m_glXCreateContextProc m_glXCreateContext;
    m_glXGetProcAddressProc m_glXGetProcAddress;
    m_glXCreateContextAttribsARBProc m_glXCreateContextAttribsARB;

    m_XOpenDisplayProc m_XOpenDisplay;
    m_XDefaultScreenProc m_XDefaultScreen;
    m_XRootWindowProc m_XRootWindow;
    m_XCreateColormapProc m_XCreateColormap;
    m_XCreateWindowProc m_XCreateWindow;
    m_XDestroyWindowProc m_XDestroyWindow;
    m_XCloseDisplayProc m_XCloseDisplay;
    m_XFreeProc m_XFree;
    m_XSetErrorHandlerProc m_XSetErrorHandler;
};

PyTypeObject * GLContext_type;

GLContext * meth_create_context(PyObject * self, PyObject * args, PyObject * kwargs) {
    static char * keywords[] = {"mode", "libgl", "libx11", "glversion", NULL};

    const char * mode = "detect";
    const char * libgl = "libGL.so";
    const char * libx11 = "libX11.so";
    int glversion = 330;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|sssi", keywords, &mode, &libgl, &libx11, &glversion)) {
        return NULL;
    }

    GLContext * res = PyObject_New(GLContext, GLContext_type);

    res->libgl = dlopen(libgl, RTLD_LAZY);
    if (!res->libgl) {
        PyErr_Format(PyExc_Exception, "%s not found in /lib, /usr/lib or LD_LIBRARY_PATH", libgl);
        return NULL;
    }

    res->m_glXChooseFBConfig = (m_glXChooseFBConfigProc)dlsym(res->libgl, "glXChooseFBConfig");
    if (!res->m_glXChooseFBConfig) {
        PyErr_Format(PyExc_Exception, "glXChooseFBConfig not found");
        return NULL;
    }

    res->m_glXChooseVisual = (m_glXChooseVisualProc)dlsym(res->libgl, "glXChooseVisual");
    if (!res->m_glXChooseVisual) {
        PyErr_Format(PyExc_Exception, "glXChooseVisual not found");
        return NULL;
    }

    res->m_glXGetCurrentDisplay = (m_glXGetCurrentDisplayProc)dlsym(res->libgl, "glXGetCurrentDisplay");
    if (!res->m_glXGetCurrentDisplay) {
        PyErr_Format(PyExc_Exception, "glXGetCurrentDisplay not found");
        return NULL;
    }

    res->m_glXGetCurrentContext = (m_glXGetCurrentContextProc)dlsym(res->libgl, "glXGetCurrentContext");
    if (!res->m_glXGetCurrentContext) {
        PyErr_Format(PyExc_Exception, "glXGetCurrentContext not found");
        return NULL;
    }

    res->m_glXGetCurrentDrawable = (m_glXGetCurrentDrawableProc)dlsym(res->libgl, "glXGetCurrentDrawable");
    if (!res->m_glXGetCurrentDrawable) {
        PyErr_Format(PyExc_Exception, "glXGetCurrentDrawable not found");
        return NULL;
    }

    res->m_glXMakeCurrent = (m_glXMakeCurrentProc)dlsym(res->libgl, "glXMakeCurrent");
    if (!res->m_glXMakeCurrent) {
        PyErr_Format(PyExc_Exception, "glXMakeCurrent not found");
        return NULL;
    }

    res->m_glXDestroyContext = (m_glXDestroyContextProc)dlsym(res->libgl, "glXDestroyContext");
    if (!res->m_glXDestroyContext) {
        PyErr_Format(PyExc_Exception, "glXDestroyContext not found");
        return NULL;
    }

    res->m_glXCreateContext = (m_glXCreateContextProc)dlsym(res->libgl, "glXCreateContext");
    if (!res->m_glXCreateContext) {
        PyErr_Format(PyExc_Exception, "glXCreateContext not found");
        return NULL;
    }

    res->m_glXGetProcAddress = (m_glXGetProcAddressProc)dlsym(res->libgl, "glXGetProcAddress");
    if (!res->m_glXGetProcAddress) {
        PyErr_Format(PyExc_Exception, "glXGetProcAddress not found");
        return NULL;
    }

    if (strcmp(mode, "detect")) {
        res->libx11 = dlopen(libx11, RTLD_LAZY);
        if (!res->libx11) {
            PyErr_Format(PyExc_Exception, "(detect) %s not loaded", libx11);
            return NULL;
        }

        res->m_XOpenDisplay = (m_XOpenDisplayProc)dlsym(res->libx11, "XOpenDisplay");
        if (!res->m_XOpenDisplay) {
            PyErr_Format(PyExc_Exception, "(detect) XOpenDisplay not found");
            return NULL;
        }

        res->m_XDefaultScreen = (m_XDefaultScreenProc)dlsym(res->libx11, "XDefaultScreen");
        if (!res->m_XDefaultScreen) {
            PyErr_Format(PyExc_Exception, "(detect) XDefaultScreen not found");
            return NULL;
        }

        res->m_XRootWindow = (m_XRootWindowProc)dlsym(res->libx11, "XRootWindow");
        if (!res->m_XRootWindow) {
            PyErr_Format(PyExc_Exception, "(detect) XRootWindow not found");
            return NULL;
        }

        res->m_XCreateColormap = (m_XCreateColormapProc)dlsym(res->libx11, "XCreateColormap");
        if (!res->m_XCreateColormap) {
            PyErr_Format(PyExc_Exception, "(detect) XCreateColormap not found");
            return NULL;
        }

        res->m_XCreateWindow = (m_XCreateWindowProc)dlsym(res->libx11, "XCreateWindow");
        if (!res->m_XCreateWindow) {
            PyErr_Format(PyExc_Exception, "(detect) XCreateWindow not found");
            return NULL;
        }

        res->m_XDestroyWindow = (m_XDestroyWindowProc)dlsym(res->libx11, "XDestroyWindow");
        if (!res->m_XDestroyWindow) {
            PyErr_Format(PyExc_Exception, "(detect) XDestroyWindow not found");
            return NULL;
        }

        res->m_XCloseDisplay = (m_XCloseDisplayProc)dlsym(res->libx11, "XCloseDisplay");
        if (!res->m_XCloseDisplay) {
            PyErr_Format(PyExc_Exception, "(detect) XCloseDisplay not found");
            return NULL;
        }

        res->m_XFree = (m_XFreeProc)dlsym(res->libx11, "XFree");
        if (!res->m_XFree) {
            PyErr_Format(PyExc_Exception, "(detect) XFree not found");
            return NULL;
        }

        res->m_XSetErrorHandler = (m_XSetErrorHandlerProc)dlsym(res->libx11, "XSetErrorHandler");
        if (!res->m_XSetErrorHandler) {
            PyErr_Format(PyExc_Exception, "(detect) XSetErrorHandler not found");
            return NULL;
        }
    }

    if (!strcmp(mode, "detect")) {
        res->standalone = false;
        res->own_window = false;

        res->ctx = res->m_glXGetCurrentContext();
        if (!res->ctx) {
            PyErr_Format(PyExc_Exception, "(detect) glXGetCurrentContext: cannot detect OpenGL context");
            return NULL;
        }

        res->wnd = res->m_glXGetCurrentDrawable();
        if (!res->wnd) {
            PyErr_Format(PyExc_Exception, "(detect) glXGetCurrentDrawable failed");
            return NULL;
        }

        res->dpy = res->m_glXGetCurrentDisplay();
        if (!res->dpy) {
            PyErr_Format(PyExc_Exception, "(detect) glXGetCurrentDisplay failed");
            return NULL;
        }

        res->fbc = NULL;
        res->vi = NULL;
        return res;
    }

    if (!strcmp(mode, "share")) {
        res->standalone = true;
        res->own_window = false;

        GLXContext ctx_share = res->m_glXGetCurrentContext();
        if (!ctx_share) {
            PyErr_Format(PyExc_Exception, "(share) glXGetCurrentContext: cannot detect OpenGL context");
            return NULL;
        }

        res->wnd = res->m_glXGetCurrentDrawable();
        if (!res->wnd) {
            PyErr_Format(PyExc_Exception, "(share) glXGetCurrentDrawable failed");
            return NULL;
        }

        res->dpy = res->m_glXGetCurrentDisplay();
        if (!res->dpy) {
            PyErr_Format(PyExc_Exception, "(share) glXGetCurrentDisplay failed");
            return NULL;
        }

        int nelements = 0;
        res->fbc = res->m_glXChooseFBConfig(res->dpy, res->m_XDefaultScreen(res->dpy), 0, &nelements);

        if (!res->fbc) {
            res->m_XCloseDisplay(res->dpy);
            PyErr_Format(PyExc_Exception, "(share) glXChooseFBConfig failed");
            return NULL;
        }

        static int attribute_list[] = {
            GLX_RGBA,
            GLX_DOUBLEBUFFER,
            GLX_RED_SIZE, 8,
            GLX_GREEN_SIZE, 8,
            GLX_BLUE_SIZE, 8,
            GLX_DEPTH_SIZE, 24,
            None,
        };

        res->vi = res->m_glXChooseVisual(res->dpy, res->m_XDefaultScreen(res->dpy), attribute_list);

        if (!res->vi) {
            res->m_XCloseDisplay(res->dpy);
            PyErr_Format(PyExc_Exception, "(share) glXChooseVisual:  cannot choose visual");
            return NULL;
        }

        res->m_XSetErrorHandler(SilentXErrorHandler);

        if (glversion) {
            void (* proc)() = res->m_glXGetProcAddress((const unsigned char *)"glXCreateContextAttribsARB");
            res->m_glXCreateContextAttribsARB = (m_glXCreateContextAttribsARBProc)proc;
            if (!res->m_glXCreateContextAttribsARB) {
                PyErr_Format(PyExc_Exception, "(share) glXCreateContextAttribsARB not found");
                return NULL;
            }

            int attribs[] = {
                GLX_CONTEXT_PROFILE_MASK, GLX_CONTEXT_CORE_PROFILE_BIT,
                GLX_CONTEXT_MAJOR_VERSION, glversion / 100 % 10,
                GLX_CONTEXT_MINOR_VERSION, glversion / 10 % 10,
                0, 0,
            };

            res->ctx = res->m_glXCreateContextAttribsARB(res->dpy, *res->fbc, ctx_share, true, attribs);
        } else {
            res->ctx = res->m_glXCreateContext(res->dpy, res->vi, ctx_share, true);
        }

        if (!res->ctx) {
            PyErr_Format(PyExc_Exception, "(share) cannot create context");
            return NULL;
        }

        res->m_XSetErrorHandler(NULL);

        if (!res->m_glXMakeCurrent(res->dpy, res->wnd, res->ctx)) {
            PyErr_Format(PyExc_Exception, "(share) glXMakeCurrent failed");
            return NULL;
        }

        return res;
    }

    if (!strcmp(mode, "standalone")) {
        res->standalone = true;
        res->own_window = true;

        res->dpy = res->m_XOpenDisplay(NULL);

        if (!res->dpy) {
            res->dpy = res->m_XOpenDisplay(":0.0");
        }

        if (!res->dpy) {
            PyErr_Format(PyExc_Exception, "(standalone) XOpenDisplay: cannot open display");
            return NULL;
        }

        int nelements = 0;
        res->fbc = res->m_glXChooseFBConfig(res->dpy, res->m_XDefaultScreen(res->dpy), 0, &nelements);

        if (!res->fbc) {
            res->m_XCloseDisplay(res->dpy);
            PyErr_Format(PyExc_Exception, "(standalone) glXChooseFBConfig failed");
            return NULL;
        }

        static int attribute_list[] = {
            GLX_RGBA,
            GLX_DOUBLEBUFFER,
            GLX_RED_SIZE, 8,
            GLX_GREEN_SIZE, 8,
            GLX_BLUE_SIZE, 8,
            GLX_DEPTH_SIZE, 24,
            None,
        };

        res->vi = res->m_glXChooseVisual(res->dpy, res->m_XDefaultScreen(res->dpy), attribute_list);

        if (!res->vi) {
            res->m_XCloseDisplay(res->dpy);
            PyErr_Format(PyExc_Exception, "(standalone) glXChooseVisual: cannot choose visual");
            return NULL;
        }

        XSetWindowAttributes swa;
        swa.colormap = res->m_XCreateColormap(res->dpy, res->m_XRootWindow(res->dpy, res->vi->screen), res->vi->visual, AllocNone);
        swa.border_pixel = 0;
        swa.event_mask = StructureNotifyMask;

        res->wnd = res->m_XCreateWindow(
            res->dpy, res->m_XRootWindow(res->dpy, res->vi->screen), 0, 0, 1, 1, 0, res->vi->depth, InputOutput,
            res->vi->visual, CWBorderPixel | CWColormap | CWEventMask, &swa
        );

        if (!res->wnd) {
            res->m_XCloseDisplay(res->dpy);
            PyErr_Format(PyExc_Exception, "(standalone) XCreateWindow: cannot create window");
            return NULL;
        }

        res->m_XSetErrorHandler(SilentXErrorHandler);

        if (glversion) {
            void (* proc)() = res->m_glXGetProcAddress((const unsigned char *)"glXCreateContextAttribsARB");
            res->m_glXCreateContextAttribsARB = (m_glXCreateContextAttribsARBProc)proc;
            if (!res->m_glXCreateContextAttribsARB) {
                PyErr_Format(PyExc_Exception, "(standalone) glXCreateContextAttribsARB not found");
                return NULL;
            }

            int attribs[] = {
                GLX_CONTEXT_PROFILE_MASK, GLX_CONTEXT_CORE_PROFILE_BIT,
                GLX_CONTEXT_MAJOR_VERSION, glversion / 100 % 10,
                GLX_CONTEXT_MINOR_VERSION, glversion / 10 % 10,
                0, 0,
            };

            res->ctx = res->m_glXCreateContextAttribsARB(res->dpy, *res->fbc, NULL, true, attribs);
        } else {
            res->ctx = res->m_glXCreateContext(res->dpy, res->vi, NULL, true);
        }

        if (!res->ctx) {
            PyErr_Format(PyExc_Exception, "(standalone) cannot create context");
            return NULL;
        }

        res->m_XSetErrorHandler(NULL);

        if (!res->m_glXMakeCurrent(res->dpy, res->wnd, res->ctx)) {
            PyErr_Format(PyExc_Exception, "(standalone) glXMakeCurrent failed");
            return NULL;
        }

        return res;
    }

    PyErr_Format(PyExc_Exception, "unknown mode");
    return NULL;
}

PyObject * GLContext_meth_load(GLContext * self, PyObject * arg) {
    const char * method = PyUnicode_AsUTF8(arg);
    void * proc = (void *)dlsym(self->libgl, method);
    if (!proc) {
        proc = (void *)self->m_glXGetProcAddress((const unsigned char *)method);
    }
    return PyLong_FromVoidPtr(proc);
}

PyObject * GLContext_meth_enter(GLContext * self) {
    self->old_display = (void *)self->m_glXGetCurrentDisplay();
    self->old_window = (void *)self->m_glXGetCurrentDrawable();
    self->old_context = (void *)self->m_glXGetCurrentContext();
    self->m_glXMakeCurrent(self->dpy, self->wnd, self->ctx);
    Py_RETURN_NONE;
}

PyObject * GLContext_meth_exit(GLContext * self) {
    self->m_glXMakeCurrent((Display *)self->old_display, (Window)self->old_window, (GLXContext)self->old_context);    
    Py_RETURN_NONE;
}

PyObject * GLContext_meth_release(GLContext * self) {
    if (self->standalone) {
        self->m_glXMakeCurrent(self->dpy, None, NULL);
        self->m_glXDestroyContext(self->dpy, self->ctx);
    }
    if (self->own_window) {
        self->m_XDestroyWindow(self->dpy, self->wnd);
        self->m_XCloseDisplay(self->dpy);
    }
    if (self->fbc) {
        self->m_XFree(self->fbc);
        self->fbc = NULL;
    }
    if (self->vi) {
        self->m_XFree(self->vi);
        self->vi = NULL;
    }
    Py_RETURN_NONE;
}

void GLContext_dealloc(GLContext * self) {
    Py_TYPE(self)->tp_free(self);
}

PyMethodDef GLContext_methods[] = {
    {"load", (PyCFunction)GLContext_meth_load, METH_O, NULL},
    {"load_opengl_function", (PyCFunction)GLContext_meth_load, METH_O, NULL},
    {"release", (PyCFunction)GLContext_meth_release, METH_NOARGS, NULL},
    {"__enter__", (PyCFunction)GLContext_meth_enter, METH_NOARGS, NULL},
    {"__exit__", (PyCFunction)GLContext_meth_exit, METH_VARARGS, NULL},
    {},
};

PyMemberDef GLContext_members[] = {
    {"standalone", T_BOOL, offsetof(GLContext, standalone), READONLY, NULL},
    {},
};

PyType_Slot GLContext_slots[] = {
    {Py_tp_methods, GLContext_methods},
    {Py_tp_members, GLContext_members},
    {Py_tp_dealloc, (void *)GLContext_dealloc},
    {},
};

PyType_Spec GLContext_spec = {"x11.GLContext", sizeof(GLContext), 0, Py_TPFLAGS_DEFAULT, GLContext_slots};

PyMethodDef module_methods[] = {
    {"create_context", (PyCFunction)meth_create_context, METH_VARARGS | METH_KEYWORDS, NULL},
    {},
};

PyModuleDef module_def = {PyModuleDef_HEAD_INIT, "x11", NULL, -1, module_methods};

extern "C" PyObject * PyInit_x11() {
    PyObject * module = PyModule_Create(&module_def);
    GLContext_type = (PyTypeObject *)PyType_FromSpec(&GLContext_spec);
    PyModule_AddObject(module, "GLContext", (PyObject *)GLContext_type);
    return module;
}
