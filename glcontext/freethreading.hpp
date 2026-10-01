#pragma once

#include <Python.h>

// Serializes access to the state of one object for the lifetime of the variable.
// This is what the GIL used to do implicitly. Critical sections exist only in free-threaded
// builds (3.13+), with the GIL (or on older Pythons) this is a no-op.
#ifdef Py_GIL_DISABLED
struct ObjectLock {
    PyCriticalSection section;
    ObjectLock(PyObject * object) { PyCriticalSection_Begin(&section, object); }
    ~ObjectLock() { PyCriticalSection_End(&section); }
};
#else
struct ObjectLock {
    ObjectLock(PyObject *) {}
};
#endif
