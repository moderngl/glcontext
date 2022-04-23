#include <Python.h>
#include <structmember.h>

#include <OpenGL/OpenGL.h>
#include <ApplicationServices/ApplicationServices.h>

#import <mach-o/dyld.h>
#import <stdlib.h>
#import <string.h>

#include <OpenGL/gl.h>
#include <OpenGL/glext.h>

struct GLContext {
    PyObject_HEAD
    CGLContextObj ctx;
    int standalone;
    void * old_context;
};

PyTypeObject * GLContext_type;

GLContext * meth_create_context(PyObject * self, PyObject * args, PyObject * kwargs) {
    static char * keywords[] = {"mode", NULL};

    const char * mode = "detect";

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|s", keywords, &mode)) {
        return NULL;
    }

    GLContext * res = PyObject_New(GLContext, GLContext_type);

    if (!strcmp(mode, "detect")) {
        res->standalone = false;

        res->ctx = CGLGetCurrentContext();
        if (!res->ctx) {
            PyErr_Format(PyExc_Exception, "cannot detect OpenGL context");
            return NULL;
        }

        return res;
    }

    if (!strcmp(mode, "standalone")) {
        res->standalone = true;

        GLint num_pixelformats = 0;
        CGLPixelFormatObj pixelformat = 0;

        CGLPixelFormatAttribute attribs[] = {
            kCGLPFAOpenGLProfile,
            (CGLPixelFormatAttribute)kCGLOGLPVersion_GL4_Core,
            (CGLPixelFormatAttribute)0,
        };

        CGLChoosePixelFormat(attribs, &pixelformat, &num_pixelformats);

        if (!pixelformat) {
            CGLPixelFormatAttribute attribs[] = {
                kCGLPFAOpenGLProfile,
                (CGLPixelFormatAttribute)kCGLOGLPVersion_GL3_Core,
                (CGLPixelFormatAttribute)0,
            };

            CGLChoosePixelFormat(attribs, &pixelformat, &num_pixelformats);

            if (!pixelformat) {
                CGLPixelFormatAttribute attribs[] = {
                    (CGLPixelFormatAttribute)0,
                };

                CGLChoosePixelFormat(attribs, &pixelformat, &num_pixelformats);
            }
        }

        if (!pixelformat) {
            PyErr_Format(PyExc_Exception, "cannot choose pixel format");
            return NULL;
        }

        CGLContextObj cgl_context = NULL;
        CGLCreateContext(pixelformat, NULL, &cgl_context);
        CGLDestroyPixelFormat(pixelformat);

        if (!cgl_context) {
            PyErr_Format(PyExc_Exception, "cannot create OpenGL context");
            return NULL;
        }
        res->ctx = cgl_context;

        CGLSetCurrentContext(cgl_context);
        return res;
    }

    PyErr_Format(PyExc_Exception, "unknown mode");
    return NULL;
}

PyObject * GLContext_meth_load(GLContext * self, PyObject * arg) {
    PyObject * prefix = PyUnicode_FromString("_");
    PyObject * prefixed = PyNumber_Add(prefix, arg);
    NSSymbol symbol = NULL;
    const char * method = PyUnicode_AsUTF8(prefixed);
    if (NSIsSymbolNameDefined(method)) {
        symbol = NSLookupAndBindSymbol(method);
    }
    Py_DECREF(prefixed);
    Py_DECREF(prefix);
    return PyLong_FromVoidPtr(symbol ? NSAddressOfSymbol(symbol) : NULL);
}

PyObject * GLContext_meth_enter(GLContext * self) {
    self->old_context = (void *)CGLGetCurrentContext();
    CGLSetCurrentContext(self->ctx);
    Py_RETURN_NONE;
}

PyObject * GLContext_meth_exit(GLContext * self) {
    CGLSetCurrentContext((CGLContextObj)self->old_context);
    Py_RETURN_NONE;
}

PyObject * GLContext_meth_release(GLContext * self) {
    if (self->standalone) {
        CGLSetCurrentContext(NULL);
        CGLDestroyContext(self->ctx);
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

PyType_Spec GLContext_spec = {"darwin.GLContext", sizeof(GLContext), 0, Py_TPFLAGS_DEFAULT, GLContext_slots};

PyMethodDef module_methods[] = {
    {"create_context", (PyCFunction)meth_create_context, METH_VARARGS | METH_KEYWORDS, NULL},
    {},
};

PyModuleDef module_def = {PyModuleDef_HEAD_INIT, "darwin", NULL, -1, module_methods};

extern "C" PyObject * PyInit_darwin() {
    PyObject * module = PyModule_Create(&module_def);
    GLContext_type = (PyTypeObject *)PyType_FromSpec(&GLContext_spec);
    PyModule_AddObject(module, "GLContext", (PyObject *)GLContext_type);
    return module;
}
