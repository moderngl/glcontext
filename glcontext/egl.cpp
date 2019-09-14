#include <Python.h>

#include <dlfcn.h>

struct Display;

typedef unsigned int EGLenum;
typedef int EGLint;
typedef unsigned int EGLBoolean;
typedef Display * EGLNativeDisplayType;
typedef void * EGLConfig;
typedef void * EGLSurface;
typedef void * EGLContext;
typedef void * EGLDeviceEXT;

#define EGL_DEFAULT_DISPLAY 0
#define EGL_NO_CONTEXT 0
#define EGL_NO_SURFACE 0
#define EGL_PBUFFER_BIT 0x0001
#define EGL_RENDERABLE_TYPE 0x3040
#define EGL_NONE 0x3038
#define EGL_OPENGL_BIT 0x0008
#define EGL_BLUE_SIZE 0x3022
#define EGL_DEPTH_SIZE 0x3025
#define EGL_RED_SIZE 0x3024
#define EGL_GREEN_SIZE 0x3023
#define EGL_SURFACE_TYPE 0x3033
#define EGL_OPENGL_API 0x30A2
#define EGL_WIDTH 0x3057
#define EGL_HEIGHT 0x3056
#define EGL_SUCCESS 0x3000
#define EGL_CONTEXT_MAJOR_VERSION 0x3098
#define EGL_CONTEXT_MINOR_VERSION 0x30FB
#define EGL_CONTEXT_OPENGL_PROFILE_MASK 0x30FD
#define EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT 0x00000001
#define EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE 0x31B1
#define EGL_PLATFORM_DEVICE_EXT 0x313F
#define EGL_PLATFORM_WAYLAND_EXT 0x31D8
#define EGL_PLATFORM_X11_EXT 0x31D5

typedef void * EGLDisplay;
typedef EGLint (* m_eglGetErrorProc)();
typedef EGLDisplay (* m_eglGetDisplayProc)(EGLNativeDisplayType);
typedef EGLBoolean (* m_eglInitializeProc)(EGLDisplay, EGLint *, EGLint *);
typedef EGLBoolean (* m_eglChooseConfigProc)(EGLDisplay, const EGLint *, EGLConfig *, EGLint, EGLint *);
typedef EGLBoolean (* m_eglBindAPIProc)(EGLenum);
typedef EGLContext (* m_eglCreateContextProc)(EGLDisplay, EGLConfig, EGLContext, const EGLint *);
typedef EGLBoolean (* m_eglDestroyContextProc)(EGLDisplay, EGLContext);
typedef EGLBoolean (* m_eglMakeCurrentProc)(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
typedef void (* (* m_eglGetProcAddressProc)(const char *))();
typedef EGLBoolean (* m_eglQueryDevicesEXTProc)(EGLint, EGLDeviceEXT *, EGLint *);
typedef EGLDisplay (* m_eglGetPlatformDisplayEXTProc) (EGLenum, void *, const EGLint *);

struct GLContext {
    PyObject_HEAD

    void * libgl;
    void * libegl;
    EGLContext ctx;
    EGLDisplay dpy;
    EGLConfig cfg;

    bool standalone;

    m_eglGetErrorProc m_eglGetError;
    m_eglGetDisplayProc m_eglGetDisplay;
    m_eglInitializeProc m_eglInitialize;
    m_eglChooseConfigProc m_eglChooseConfig;
    m_eglBindAPIProc m_eglBindAPI;
    m_eglCreateContextProc m_eglCreateContext;
    m_eglDestroyContextProc m_eglDestroyContext;
    m_eglMakeCurrentProc m_eglMakeCurrent;
    m_eglGetProcAddressProc m_eglGetProcAddress;
    m_eglQueryDevicesEXTProc m_eglQueryDevicesEXT;
    m_eglGetPlatformDisplayEXTProc m_eglGetPlatformDisplayEXT;
};

PyTypeObject * GLContext_type;

GLContext * meth_create_context(PyObject * self, PyObject * args, PyObject * kwargs) {
    static char * keywords[] = {"mode", "libgl", "libegl", "glversion", NULL};

    const char * mode = "standalone";
    const char * libgl = "libGL.so";
    const char * libegl = "libEGL.so";
    int glversion = 330;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|sssi", keywords, &mode, &libgl, &libegl, &glversion)) {
        return NULL;
    }

    GLContext * res = PyObject_New(GLContext, GLContext_type);

    res->libgl = dlopen(libgl, RTLD_LAZY);
    if (!res->libgl) {
        PyErr_Format(PyExc_Exception, "%s not loaded", libgl);
        return NULL;
    }

    res->libegl = dlopen(libegl, RTLD_LAZY);
    if (!res->libegl) {
        PyErr_Format(PyExc_Exception, "%s not loaded", libegl);
        return NULL;
    }

    res->m_eglGetError = (m_eglGetErrorProc)dlsym(res->libegl, "eglGetError");
    if (!res->m_eglGetError) {
        PyErr_Format(PyExc_Exception, "eglGetError not found");
        return NULL;
    }

    res->m_eglGetDisplay = (m_eglGetDisplayProc)dlsym(res->libegl, "eglGetDisplay");
    if (!res->m_eglGetDisplay) {
        PyErr_Format(PyExc_Exception, "eglGetDisplay not found");
        return NULL;
    }

    res->m_eglInitialize = (m_eglInitializeProc)dlsym(res->libegl, "eglInitialize");
    if (!res->m_eglInitialize) {
        PyErr_Format(PyExc_Exception, "eglInitialize not found");
        return NULL;
    }

    res->m_eglChooseConfig = (m_eglChooseConfigProc)dlsym(res->libegl, "eglChooseConfig");
    if (!res->m_eglChooseConfig) {
        PyErr_Format(PyExc_Exception, "eglChooseConfig not found");
        return NULL;
    }

    res->m_eglBindAPI = (m_eglBindAPIProc)dlsym(res->libegl, "eglBindAPI");
    if (!res->m_eglBindAPI) {
        PyErr_Format(PyExc_Exception, "eglBindAPI not found");
        return NULL;
    }

    res->m_eglCreateContext = (m_eglCreateContextProc)dlsym(res->libegl, "eglCreateContext");
    if (!res->m_eglCreateContext) {
        PyErr_Format(PyExc_Exception, "eglCreateContext not found");
        return NULL;
    }

    res->m_eglDestroyContext = (m_eglDestroyContextProc)dlsym(res->libegl, "eglDestroyContext");
    if (!res->m_eglDestroyContext) {
        PyErr_Format(PyExc_Exception, "eglDestroyContext not found");
        return NULL;
    }

    res->m_eglMakeCurrent = (m_eglMakeCurrentProc)dlsym(res->libegl, "eglMakeCurrent");
    if (!res->m_eglMakeCurrent) {
        PyErr_Format(PyExc_Exception, "eglMakeCurrent not found");
        return NULL;
    }

    res->m_eglGetProcAddress = (m_eglGetProcAddressProc)dlsym(res->libegl, "eglGetProcAddress");
    if (!res->m_eglGetProcAddress) {
        PyErr_Format(PyExc_Exception, "eglGetProcAddress not found");
        return NULL;
    }

    // res->m_eglQueryDevicesEXT = (m_eglQueryDevicesEXTProc)res->m_eglGetProcAddress("eglQueryDevicesEXT");
    // if (!res->m_eglQueryDevicesEXT) {
    //     return NULL;
    // }

    // res->m_eglGetPlatformDisplayEXT = (m_eglGetPlatformDisplayEXTProc)res->m_eglGetProcAddress("eglGetPlatformDisplayEXT");
    // if (!res->m_eglGetPlatformDisplayEXT) {
    //     return NULL;
    // }

    if (!strcmp(mode, "standalone")) {
        res->standalone = true;

        res->dpy = res->m_eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (!res->dpy) {
            PyErr_Format(PyExc_Exception, "eglGetDisplay failed (0x%x)", res->m_eglGetError());
            return NULL;
        }

        EGLint major, minor;
        if (!res->m_eglInitialize(res->dpy, &major, &minor)) {
            PyErr_Format(PyExc_Exception, "eglInitialize failed (0x%x)", res->m_eglGetError());
            return NULL;
        }

        EGLint config_attribs[] = {
            EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_DEPTH_SIZE, 8,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
            EGL_NONE
        };

        EGLint num_configs = 0;
        if (!res->m_eglChooseConfig(res->dpy, config_attribs, &res->cfg, 1, &num_configs)) {
            PyErr_Format(PyExc_Exception, "eglChooseConfig failed (0x%x)", res->m_eglGetError());
            return NULL;
        }

        if (!res->m_eglBindAPI(EGL_OPENGL_API)) {
            PyErr_Format(PyExc_Exception, "eglBindAPI failed (0x%x)", res->m_eglGetError());
            return NULL;
        }

        int ctxattribs[] = {
            EGL_CONTEXT_MAJOR_VERSION, glversion / 100 % 10,
            EGL_CONTEXT_MINOR_VERSION, glversion / 10 % 10,
            EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
            // EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE, 1,
            EGL_NONE,
        };

        res->ctx = res->m_eglCreateContext(res->dpy, res->cfg, EGL_NO_CONTEXT, ctxattribs);
        if (!res->ctx) {
            PyErr_Format(PyExc_Exception, "eglCreateContext failed (0x%x)", res->m_eglGetError());
            return NULL;
        }

        res->m_eglMakeCurrent(res->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, res->ctx);
        return res;
    }

    PyErr_Format(PyExc_Exception, "unknown mode");
    return NULL;
}

PyObject * GLContext_meth_load(GLContext * self, PyObject * arg) {
    const char * method = PyUnicode_AsUTF8(arg);
    void * proc = (void *)dlsym(self->libgl, method);
    if (!proc) {
        proc = (void *)self->m_eglGetProcAddress(method);
    }
    return PyLong_FromVoidPtr(proc);
}

PyObject * GLContext_meth_enter(GLContext * self) {
    self->m_eglMakeCurrent(self->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, self->ctx);
    Py_RETURN_NONE;
}

PyObject * GLContext_meth_exit(GLContext * self) {
    self->m_eglMakeCurrent(self->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    Py_RETURN_NONE;
}

PyObject * GLContext_meth_release(GLContext * self) {
    self->m_eglDestroyContext(self->dpy, self->ctx);
    Py_RETURN_NONE;
}

void GLContext_dealloc(GLContext * self) {
    Py_TYPE(self)->tp_free(self);
}

PyMethodDef GLContext_methods[] = {
    {"load", (PyCFunction)GLContext_meth_load, METH_O, NULL},
    {"release", (PyCFunction)GLContext_meth_release, METH_NOARGS, NULL},
    {"__enter__", (PyCFunction)GLContext_meth_enter, METH_NOARGS, NULL},
    {"__exit__", (PyCFunction)GLContext_meth_exit, METH_VARARGS, NULL},
    {},
};

PyType_Slot GLContext_slots[] = {
    {Py_tp_methods, GLContext_methods},
    {Py_tp_dealloc, GLContext_dealloc},
    {},
};

PyType_Spec GLContext_spec = {"egl.GLContext", sizeof(GLContext), 0, Py_TPFLAGS_DEFAULT, GLContext_slots};

PyMethodDef module_methods[] = {
    {"create_context", (PyCFunction)meth_create_context, METH_VARARGS | METH_KEYWORDS, NULL},
    {},
};

PyModuleDef module_def = {PyModuleDef_HEAD_INIT, "egl", NULL, -1, module_methods};

extern "C" PyObject * PyInit_egl() {
    PyObject * module = PyModule_Create(&module_def);
    GLContext_type = (PyTypeObject *)PyType_FromSpec(&GLContext_spec);
    PyModule_AddObject(module, "GLContext", (PyObject *)GLContext_type);
    return module;
}
