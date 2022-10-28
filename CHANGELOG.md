
# Change Log

## 2.3.7

Python 3.11 support

## 2.3.6

Expose headless/standalone flag

## 2.3.3

* Missing manylinux wheels for python 3.9
* Minor issue in setup.py

## 2.3.0

python 3.9 support

## 2.3.dev0

* EGL backend will now use `eglQueryDevicesEXT` instead of only relying on `EGL_DEFAULT_DISPLAY`
* EGL backend now supports `device_index` for selecting a device

## 2.2.0

* x11 and egl backend will now use `ctypes.utils.find_library`
  to locate GL and EGL if not `libgl` and `libegl` parameter
  is passed to the backend

## 2.1.0

* Support setting backend arguments using environment variables.
  * `GLCONTEXT_GLVERSION` for setting opengl version
  * `GLCONTEXT_LINUX_LIBGL` for specifying libgl name
  * `GLCONTEXT_LINUX_LIBX11` for specifying libx11 name
  * `GLCONTEXT_LINUX_LIBEGL` for specifying libegl name
  * `GLCONTEXT_WIN_LIBGL` for specifying dll name
* x11: More details in error messages

## 2.0.0

Support passing in values to backends for more detailed
configuration. Method signatures have changed so upgrading
from 1.* needs smaller code changes.

- `default_backend()` no longer takes any arguments
- The returned backend now takes `glversion` and other arguments
- The `standalone` argument is now called `mode` and can contain
  `standalone`, `share` and `detect`.
- Added `get_backend` for requesting specific backends like EGL.

## 1.0.1

* darwin: Fixed a segfault when releasing a context
* x11: Fixed an issue causing context creation to fail

## 1.0.0

Initial release. Contains backends for wgl, darwin and x11
including experimental egl backend.
