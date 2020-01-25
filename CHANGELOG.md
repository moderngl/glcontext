
# Change Log

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
