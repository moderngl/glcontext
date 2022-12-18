#include <Python.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

int num_devices;
EGLDeviceEXT devices[64];

EGLContext context;
EGLDisplay display;
EGLConfig config;

PyObject * meth_devices(PyObject * self) {
    PFNEGLQUERYDEVICESEXTPROC eglQueryDevicesEXT = (PFNEGLQUERYDEVICESEXTPROC)eglGetProcAddress("eglQueryDevicesEXT");
    PFNEGLQUERYDEVICESTRINGEXTPROC eglQueryDeviceStringEXT = (PFNEGLQUERYDEVICESTRINGEXTPROC)eglGetProcAddress("eglQueryDeviceStringEXT");

    if (!eglQueryDevicesEXT(0, NULL, &num_devices)) {
        return NULL;
    }

    if (!eglQueryDevicesEXT(num_devices, devices, &num_devices)) {
        return NULL;
    }

    PyObject * res = PyList_New(num_devices);
    for (int i = 0; i < num_devices; ++i) {
        const char * egl_extensions = eglQueryDeviceStringEXT(devices[i], EGL_EXTENSIONS);
        PyObject * temp = PyUnicode_FromString(egl_extensions ? egl_extensions : "");
        PyObject * extensions = PyObject_CallMethod(temp, "split", NULL);
        Py_DECREF(temp);
        PyList_SetItem(res, i, Py_BuildValue("{sisN}", "device", i, "extensions", extensions));
    }
    return res;
}

PyObject * meth_init(PyObject * self, PyObject * args, PyObject * kwargs) {
    const char * keywords[] = {"device", NULL};

    int device = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "i", (char **)keywords, &device)) {
        return NULL;
    }

    if (device > num_devices) {
        return NULL;
    }

    display = eglGetPlatformDisplay(EGL_PLATFORM_DEVICE_EXT, devices[device], 0);
    if (display == EGL_NO_DISPLAY) {
        return NULL;
    }

    if (!eglInitialize(display, NULL, NULL)) {
        return NULL;
    }

    int config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_NONE,
    };

    int num_configs = 0;
    if (!eglChooseConfig(display, config_attribs, &config, 1, &num_configs)) {
        return NULL;
    }

    if (!eglBindAPI(EGL_OPENGL_API)) {
        return NULL;
    }

    int context_attribs[] = {
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
        EGL_NONE,
    };

    context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribs);
    if (!context) {
        return NULL;
    }

    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context);
    Py_RETURN_NONE;
}

PyObject * meth_load_opengl_function(PyObject * self, PyObject * arg) {
    if (!PyUnicode_CheckExact(arg)) {
        return NULL;
    }
    const char * name = PyUnicode_AsUTF8(arg);
    return PyLong_FromVoidPtr((void *)eglGetProcAddress(name));
}

PyMethodDef module_methods[] = {
    {"devices", (PyCFunction)meth_devices, METH_NOARGS},
    {"init", (PyCFunction)meth_init, METH_VARARGS | METH_KEYWORDS},
    {"load_opengl_function", (PyCFunction)meth_load_opengl_function, METH_O},
    {},
};

PyModuleDef module_def = {PyModuleDef_HEAD_INIT, "headless", NULL, -1, module_methods};

extern "C" PyObject * PyInit_headless() {
    PyObject * module = PyModule_Create(&module_def);
    return module;
}
