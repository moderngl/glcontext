#include <Python.h>
#include <structmember.h>

#include <Windows.h>

#define WGL_CONTEXT_PROFILE_MASK 0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT 0x0001
#define WGL_CONTEXT_MAJOR_VERSION 0x2091
#define WGL_CONTEXT_MINOR_VERSION 0x2092

typedef HGLRC (WINAPI * m_wglGetCurrentContextProc)();
typedef HDC (WINAPI * m_wglGetCurrentDCProc)();
typedef HGLRC (WINAPI * m_wglCreateContextProc)(HDC);
typedef BOOL (WINAPI * m_wglDeleteContextProc)(HGLRC);
typedef PROC (WINAPI * m_wglGetProcAddressProc)(LPCSTR);
typedef BOOL (WINAPI * m_wglMakeCurrentProc)(HDC, HGLRC);
typedef HGLRC (WINAPI * m_wglCreateContextAttribsARBProc)(HDC, HGLRC, const int *);
typedef BOOL (WINAPI * m_wglSwapIntervalEXTProc)(int);

HINSTANCE hinst;

struct GLContext {
    PyObject_HEAD

    HMODULE libgl;
    HWND hwnd;
    HDC hdc;
    HGLRC hrc;

    int standalone;
    void * old_context;
    void * old_display;

    m_wglGetCurrentContextProc m_wglGetCurrentContext;
    m_wglGetCurrentDCProc m_wglGetCurrentDC;
    m_wglCreateContextProc m_wglCreateContext;
    m_wglDeleteContextProc m_wglDeleteContext;
    m_wglGetProcAddressProc m_wglGetProcAddress;
    m_wglMakeCurrentProc m_wglMakeCurrent;
    m_wglCreateContextAttribsARBProc m_wglCreateContextAttribsARB;
    m_wglSwapIntervalEXTProc m_wglSwapIntervalEXT;
};

PyTypeObject * GLContext_type;

GLContext * meth_create_context(PyObject * self, PyObject * args, PyObject * kwargs) {
    static char * keywords[] = {"mode", "libgl", "glversion", NULL};

    const char * mode = "detect";
    const char * libgl = "opengl32.dll";
    int glversion = 330;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|ssi", keywords, &mode, &libgl, &glversion)) {
        return NULL;
    }

    GLContext * res = PyObject_New(GLContext, GLContext_type);

    // The mode parameter is required for dll's specifed as libgl
    // to load successfully along with its dependencies on Python 3.8+.
    // This is due to the introduction of using a short dll load path
    // inside of Python 3.8+.

    int load_mode = LOAD_LIBRARY_SEARCH_DEFAULT_DIRS; // Search default dirs first

    // Check whether libgl contains `/` or `\\`.
    // We can treat that `libgl` would be absolute in that case.
    if (strchr(libgl, '/') || strchr(libgl, '\\'))
    {
        // Search the dirs in which the dll is located
        load_mode |= LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR;
    }
    res->libgl = LoadLibraryEx(libgl, NULL, (DWORD)load_mode);
    if (!res->libgl) {
        DWORD last_error = GetLastError();
        PyErr_Format(PyExc_Exception, "%s not loaded. Error code: %ld.", libgl, last_error);
        return NULL;
    }

    res->m_wglGetCurrentContext = (m_wglGetCurrentContextProc)GetProcAddress(res->libgl, "wglGetCurrentContext");
    if (!res->m_wglGetCurrentContext) {
        PyErr_Format(PyExc_Exception, "wglGetCurrentContext not found");
        return NULL;
    }

    res->m_wglGetCurrentDC = (m_wglGetCurrentDCProc)GetProcAddress(res->libgl, "wglGetCurrentDC");
    if (!res->m_wglGetCurrentDC) {
        PyErr_Format(PyExc_Exception, "wglGetCurrentDC not found");
        return NULL;
    }

    res->m_wglCreateContext = (m_wglCreateContextProc)GetProcAddress(res->libgl, "wglCreateContext");
    if (!res->m_wglCreateContext) {
        PyErr_Format(PyExc_Exception, "wglCreateContext not found");
        return NULL;
    }

    res->m_wglDeleteContext = (m_wglDeleteContextProc)GetProcAddress(res->libgl, "wglDeleteContext");
    if (!res->m_wglDeleteContext) {
        PyErr_Format(PyExc_Exception, "wglDeleteContext not found");
        return NULL;
    }

    res->m_wglGetProcAddress = (m_wglGetProcAddressProc)GetProcAddress(res->libgl, "wglGetProcAddress");
    if (!res->m_wglGetProcAddress) {
        PyErr_Format(PyExc_Exception, "wglGetProcAddress not found");
        return NULL;
    }

    res->m_wglMakeCurrent = (m_wglMakeCurrentProc)GetProcAddress(res->libgl, "wglMakeCurrent");
    if (!res->m_wglMakeCurrent) {
        PyErr_Format(PyExc_Exception, "wglMakeCurrent not found");
        return NULL;
    }

    if (!strcmp(mode, "detect")) {
        res->hwnd = NULL;

        res->hrc = res->m_wglGetCurrentContext();
        if (!res->hrc) {
            PyErr_Format(PyExc_Exception, "cannot detect OpenGL context");
            return NULL;
        }

        res->hdc = res->m_wglGetCurrentDC();
        if (!res->hdc) {
            PyErr_Format(PyExc_Exception, "wglGetCurrentDC failed");
            return NULL;
        }

        return res;
    }

    if (!strcmp(mode, "share")) {
        res->hwnd = NULL;
        res->standalone = true;

        HGLRC hrc_share = res->m_wglGetCurrentContext();
        if (!hrc_share) {
            PyErr_Format(PyExc_Exception, "cannot detect OpenGL context");
            return NULL;
        }

        res->hdc = res->m_wglGetCurrentDC();
        if (!res->hdc) {
            PyErr_Format(PyExc_Exception, "wglGetCurrentDC failed");
            return NULL;
        }

        FARPROC proc = res->m_wglGetProcAddress("wglCreateContextAttribsARB");
        res->m_wglCreateContextAttribsARB = (m_wglCreateContextAttribsARBProc)proc;
        if (!res->m_wglCreateContextAttribsARB) {
            PyErr_Format(PyExc_Exception, "wglCreateContextAttribsARB failed");
            return NULL;
        }

        res->m_wglMakeCurrent(NULL, NULL);

        int attribs[] = {
            WGL_CONTEXT_PROFILE_MASK, WGL_CONTEXT_CORE_PROFILE_BIT,
            WGL_CONTEXT_MAJOR_VERSION, glversion / 100 % 10,
            WGL_CONTEXT_MINOR_VERSION, glversion / 10 % 10,
            0, 0,
        };

        res->hrc = res->m_wglCreateContextAttribsARB(res->hdc, hrc_share, attribs);
        if (!res->hrc) {
            PyErr_Format(PyExc_Exception, "wglCreateContextAttribsARB failed");
            return NULL;
        }

        if (!res->m_wglMakeCurrent(res->hdc, res->hrc)) {
            PyErr_Format(PyExc_Exception, "wglMakeCurrent failed");
            return NULL;
        }

        return res;
    }

    if (!strcmp(mode, "standalone")) {
        res->standalone = true;

        res->hwnd = CreateWindow("glcontext", NULL, 0, 0, 0, 0, 0, NULL, NULL, hinst, NULL);
        if (!res->hwnd) {
            PyErr_Format(PyExc_Exception, "CreateWindow failed");
            return NULL;
        }

        res->hdc = GetDC(res->hwnd);
        if (!res->hdc) {
            PyErr_Format(PyExc_Exception, "GetDC failed");
            return NULL;
        }

        PIXELFORMATDESCRIPTOR pfd = {
            sizeof(PIXELFORMATDESCRIPTOR),
            1,
            PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_GENERIC_ACCELERATED | PFD_DOUBLEBUFFER,
            0,
            24,
        };

        int pixelformat = ChoosePixelFormat(res->hdc, &pfd);
        if (!pixelformat) {
            PyErr_Format(PyExc_Exception, "ChoosePixelFormat failed");
            return NULL;
        }

        if (!SetPixelFormat(res->hdc, pixelformat, &pfd)) {
            PyErr_Format(PyExc_Exception, "SetPixelFormat failed");
            return NULL;
        }

        HGLRC hrc_share = res->m_wglCreateContext(res->hdc);
        if (!hrc_share) {
            PyErr_Format(PyExc_Exception, "wglCreateContext failed");
            return NULL;
        }

        if (!res->m_wglMakeCurrent(res->hdc, hrc_share)) {
            PyErr_Format(PyExc_Exception, "wglMakeCurrent failed");
            return NULL;
        }

        FARPROC proc = res->m_wglGetProcAddress("wglCreateContextAttribsARB");
        res->m_wglCreateContextAttribsARB = (m_wglCreateContextAttribsARBProc)proc;
        if (!res->m_wglCreateContextAttribsARB) {
            PyErr_Format(PyExc_Exception, "wglCreateContextAttribsARB not found");
            return NULL;
        }

        res->m_wglMakeCurrent(NULL, NULL);

        if (!res->m_wglDeleteContext(hrc_share)) {
            PyErr_Format(PyExc_Exception, "wglDeleteContext failed");
            return NULL;
        }

        int attribs[] = {
            WGL_CONTEXT_PROFILE_MASK, WGL_CONTEXT_CORE_PROFILE_BIT,
            WGL_CONTEXT_MAJOR_VERSION, glversion / 100 % 10,
            WGL_CONTEXT_MINOR_VERSION, glversion / 10 % 10,
            0, 0,
        };

        res->hrc = res->m_wglCreateContextAttribsARB(res->hdc, NULL, attribs);
        if (!res->hrc) {
            PyErr_Format(PyExc_Exception, "wglCreateContextAttribsARB failed");
            return NULL;
        }

        if (!res->m_wglMakeCurrent(res->hdc, res->hrc)) {
            PyErr_Format(PyExc_Exception, "wglMakeCurrent failed");
            return NULL;
        }

        return res;
    }

    PyErr_Format(PyExc_Exception, "unknown mode");
    return NULL;
}

PyObject * GLContext_meth_load(GLContext * self, PyObject * arg) {
    const char * name = PyUnicode_AsUTF8(arg);
    void * proc = (void *)GetProcAddress(self->libgl, name);
    if (!proc) {
        proc = (void *)self->m_wglGetProcAddress(name);
    }
    return PyLong_FromVoidPtr(proc);
}

PyObject * GLContext_meth_enter(GLContext * self) {
    self->old_context = (void *)self->m_wglGetCurrentContext();
    self->old_display = (void *)self->m_wglGetCurrentDC();
    self->m_wglMakeCurrent(self->hdc, self->hrc);
    Py_RETURN_NONE;
}

PyObject * GLContext_meth_exit(GLContext * self) {
    self->m_wglMakeCurrent((HDC)self->old_display, (HGLRC)self->old_context);
    Py_RETURN_NONE;
}

PyObject * GLContext_meth_release(GLContext * self) {
    self->m_wglMakeCurrent(NULL, NULL);
    self->m_wglDeleteContext(self->hrc);
    if (self->standalone) {
        ReleaseDC(self->hwnd, self->hdc);
        DestroyWindow(self->hwnd);
    }
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

PyType_Spec GLContext_spec = {"wgl.GLContext", sizeof(GLContext), 0, Py_TPFLAGS_DEFAULT, GLContext_slots};

PyMethodDef module_methods[] = {
    {"create_context", (PyCFunction)meth_create_context, METH_VARARGS | METH_KEYWORDS, NULL},
    {},
};

PyModuleDef module_def = {PyModuleDef_HEAD_INIT, "wgl", NULL, -1, module_methods};

extern "C" PyObject * PyInit_wgl() {
    PyObject * module = PyModule_Create(&module_def);
    GLContext_type = (PyTypeObject *)PyType_FromSpec(&GLContext_spec);
    PyModule_AddObject(module, "GLContext", (PyObject *)GLContext_type);
    return module;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        hinst = hinstDLL;

        WNDCLASSEX wnd_class = {
            sizeof(WNDCLASSEX), CS_OWNDC, DefWindowProc, 0, 0, hinst, NULL, NULL, NULL, NULL, "glcontext", NULL,
        };

        if (!RegisterClassEx(&wnd_class)) {
            return NULL;
        }
    }
    return true;
}
