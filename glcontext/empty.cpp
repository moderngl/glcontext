#include <Python.h>

struct GLContext {
    PyObject_HEAD
};

PyTypeObject * GLContext_type;

GLContext * meth_create_context(PyObject * self, PyObject * args, PyObject * kwargs) {
    static char * keywords[] = {NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "", keywords)) {
        return NULL;
    }

    GLContext * res = PyObject_New(GLContext, GLContext_type);
    return res;
}

PyObject * GLContext_meth_load(GLContext * self, PyObject * arg) {
    return PyLong_FromVoidPtr(NULL);
}

PyObject * GLContext_meth_enter(GLContext * self) {
    Py_RETURN_NONE;
}

PyObject * GLContext_meth_exit(GLContext * self) {
    Py_RETURN_NONE;
}

PyObject * GLContext_meth_release(GLContext * self) {
    Py_RETURN_NONE;
}

void GLContext_dealloc(GLContext * self) {
    Py_TYPE(self)->tp_free(self);
}

PyMethodDef GLContext_methods[] = {
    {"load_opengl_function", (PyCFunction)GLContext_meth_load, METH_O, NULL},
    {"release", (PyCFunction)GLContext_meth_release, METH_NOARGS, NULL},
    {"__enter__", (PyCFunction)GLContext_meth_enter, METH_NOARGS, NULL},
    {"__exit__", (PyCFunction)GLContext_meth_exit, METH_VARARGS, NULL},
    {},
};

PyType_Slot GLContext_slots[] = {
    {Py_tp_methods, GLContext_methods},
    {Py_tp_dealloc, (void *)GLContext_dealloc},
    {},
};

PyType_Spec GLContext_spec = {"empty.GLContext", sizeof(GLContext), 0, Py_TPFLAGS_DEFAULT, GLContext_slots};

PyMethodDef module_methods[] = {
    {"create_context", (PyCFunction)meth_create_context, METH_VARARGS | METH_KEYWORDS, NULL},
    {},
};

PyModuleDef module_def = {PyModuleDef_HEAD_INIT, "empty", NULL, -1, module_methods};

extern "C" PyObject * PyInit_empty() {
    PyObject * module = PyModule_Create(&module_def);
    GLContext_type = (PyTypeObject *)PyType_FromSpec(&GLContext_spec);
    PyModule_AddObject(module, "GLContext", (PyObject *)GLContext_type);
    return module;
}
