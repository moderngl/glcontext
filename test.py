import ctypes

from glcontext import headless

devices = headless.devices()

headless.init(device=next(x['device'] for x in devices if 'EGL_MESA_device_software' in x['extensions']))

glGetStringPtr = headless.load_opengl_function('glGetString')
glGetString = ctypes.cast(glGetStringPtr, ctypes.CFUNCTYPE(ctypes.c_char_p, ctypes.c_uint32))

print(glGetString(0x1F00).decode())
print(glGetString(0x1F01).decode())
print(glGetString(0x1F02).decode())
print(glGetString(0x1F03).decode())
