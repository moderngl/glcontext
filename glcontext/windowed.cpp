#include <Python.h>

#if defined(_WIN32) || defined(_WIN64)

#include <Windows.h>

PyObject * meth_load_opengl_function(PyObject * self, PyObject * arg) {
    if (!PyUnicode_CheckExact(arg)) {
        return NULL;
    }
    HMODULE module = GetModuleHandle("opengl32");
    if (!module) {
        return NULL;
    }
    const char * name = PyUnicode_AsUTF8(arg);
    void * proc = (void *)GetProcAddress(module, name);
    if (!proc) {
        proc = wglGetProcAddress(name);
    }
    return PyLong_FromVoidPtr(proc);
}

#elif defined(__APPLE__)

#include <OpenGL/OpenGL.h>
#include <ApplicationServices/ApplicationServices.h>

#import <mach-o/dyld.h>
#import <stdlib.h>
#import <string.h>

#include <OpenGL/gl.h>
#include <OpenGL/glext.h>

PyObject * meth_load_opengl_function(PyObject * self, PyObject * arg) {
    if (!PyUnicode_CheckExact(arg)) {
        return NULL;
    }
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

#else

#include <GL/gl.h>
#include <GL/glx.h>

PyObject * meth_load_opengl_function(PyObject * self, PyObject * arg) {
    if (!PyUnicode_CheckExact(arg)) {
        return NULL;
    }
    const char * name = PyUnicode_AsUTF8(arg);
    return PyLong_FromVoidPtr((void *)glXGetProcAddress((unsigned char *)name));
}

#endif

PyMethodDef module_methods[] = {
    {"load_opengl_function", (PyCFunction)meth_load_opengl_function, METH_O},
    {},
};

PyModuleDef module_def = {PyModuleDef_HEAD_INIT, "windowed", NULL, -1, module_methods};

extern "C" PyObject * PyInit_windowed() {
    PyObject * module = PyModule_Create(&module_def);
    return module;
}
