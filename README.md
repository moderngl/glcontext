[![pypi](https://badge.fury.io/py/glcontext.svg)](https://pypi.python.org/pypi/glcontext)

<img align="right" src="https://github.com/moderngl/glcontext/raw/master/.github/icon_small.png">

# glcontext

**glcontext** is a library providing OpenGL implementation for ModernGL on multiple platforms.

* [glcontext on github](https://github.com/moderngl/glcontext)
* [glcontext on pypi](https://pypi.org/project/glcontext)
* [ModernGL](https://github.com/moderngl/moderngl)

## Backends

A glcontext backend is either an extension or a submodule of the glcontext package.
The package itself does not import any of the backends.
Importing the base package `glcontext` must safe and lightweight.

## Structure

Every backend of glcontext must provide a factory function:

```py
def create_context(*args, **kwargs) -> GLContext:
    pass
```

The create\_context method can take any number of positional and keyword arguments.
The factory function must return an object supporting the following methods:

```py
def load(self, name:str) -> int:
    pass
```

The load method takes an OpenGL function name as an input and returns a C/C++ function pointer as a python integer.
The return value must be 0 for not implemented functions.

```py
def __enter__(self, name:str):
    pass
```

The enter method calls `___MakeCurrent` to make the GLContext the calling thread's current rendering context.
`___MakeCurrent` stands for `wglMakeCurrent`, `glxMakeCurrent`, ...

```py
def __exit__(self, exc_type, exc_val, exc_tb):
    pass
```

The exit method calls `___MakeCurrent` to make the GLContext no longer current.

```py
def release(self):
    pass
```

The release method destroys the OpenGL context.

## Development Guide

There are "empty" example backends provided for developers to help adding new backends to the library.
There is a pure python example in [empty.py](#) and an extension example in [empty.cpp](#).
Besides their name match, they do not depend on each other, they are independent submodules of glcontext.

An "portable" backend implementation must load its dependency at runtime.
This rule is for simplifying the build of the entire package.
If an implementation cannot provide a "portable" backend, it will not be added to this library.
Non "portable" backends are welcome as third party libraries.

A backend must be lightweight, its size must fit within reasonable limits.

To add support for new platforms one must edit the `setup.py` too.
Platform specific dependencies are exceptions from the "portability" rule.

Example for platform specific dependencies:

- `gdi32.lib` on windows
- `libdl.a` on linux

Please note that `libGL.so` is loaded dinamically by the backends.

## Current backends

Each backend supports a `glversion` and `mode` parameters as a minimum.
The `glversion` is the minimum OpenGL version required while `mode`
decides how the context is created.

Modes

* `detect`: Will detect an existing active OpenGL context.
* `standalone`: Crates a headless OpenGL context
* `share`: Creates a new context sharing objects with the currently active context (headless)

### wgl

Parameters

* `glversion` (`int`): The minimum OpenGL version for the context
* `mode` (`str`): Creation mode. `detect` | `standalone` | `share`
* `libgl` (`str`): Name of gl library to load (default: `opengl32.dll`)

### x11

If `libgl` is not passed in the backend will try to locate
the GL library using `ctypes.utils.find_library`.

Parameters

* `glversion` (`int`): The minimum OpenGL version for the context
* `mode` (`str`): Creation mode. `detect` | `standalone` | `share`
* `libgl` (`str`): Name of gl library to load (default: `libGL.so`)
* `libx11` (`str`): Name of x11 library to load (default: `libX11.so`)

### darwin

Will create the the highest core context available.

Parameters

* `mode` (`str`): Creation mode. `detect` | `standalone`

### egl

Only supports standalone mode.

If `libgl` and/or `libegl` is not passed in the backend will try to locate
GL and/or EGL library using `ctypes.utils.find_library`.

Parameters

* `glversion` (`int`): The minimum OpenGL version for the context
* `mode` (`str`): Creation mode. `standalone`
* `libgl` (`str`): Name of gl library to load (default: `libGL.so`)
* `libegl` (`str`): Name of gl library to load (default: `libEGL.so`)
* `device_index` (`int`) The device index to use (default: `0`)

## Environment Variables

Environment variables can be set to configure backends.
These will get first priority if defined.

```bash
# Override OpenGL version code. For example: 410 (for opengl 4.1)
GLCONTEXT_GLVERSION
# Override libgl on linux. For example: libGL.1.so
GLCONTEXT_LINUX_LIBGL
# Override libx11 on linux. For exampleØ libX11.x.so
GLCONTEXT_LINUX_LIBX11
# Override libegl on linux. For exampleØ libEGL.x.so
GLCONTEXT_LINUX_LIBEGL
# Override gl dll on windows. For example: opengl32_custom.dll
GLCONTEXT_WIN_LIBGL
# Override the device index (egl)
GLCONTEXT_DEVICE_INDEX
```

## Running tests

```
pip install -r tests/requirements.txt
pytest tests
```

## Contributing

Contribution is welcome.

Pull Requests will be merged if they match the [Development Guide](#).

For prototypes, pure python implementations using ctypes are also welcome.
We will probably port it to a proper extension in the future.

Please ask questions [here](https://github.com/moderngl/glcontext/issues).
