import os

__version__ = '2.3.7'


def default_backend():
    """Get default backend based on the detected platform.
    Supports detecting an existing context and standalone contexts.
    If no context if found for the platform we return the linux backend.

    Example::

        # Get the available backend
        backend = get_default_backend(standalone=False/True)
        # Create an opengl 3.3 context or detect the currently active
        # context requiring at least opengl 3.3 support.
        ctx = backend(330)

    Returns:
        A backend object for creating and/or detecting context
    """
    PLATFORMS = {'windows', 'linux', 'darwin'}

    import platform
    target = platform.system().lower()

    for known in PLATFORMS:
        if target.startswith(known):
            target = known

    if target not in PLATFORMS:
        target = 'linux'

    if target == 'windows':
        return _wgl()

    if target == 'linux':
        return _x11()

    if target == 'darwin':
        return _darwin()

    raise ValueError("Cannot find suitable default backend")


def get_backend_by_name(name: str):
    """Request a specific backend by name"""
    if name == 'egl':
        return _egl()

    raise ValueError("Cannot find supported backend: '{}'".format(name))


def _wgl():
    """Create wgl backend"""
    from glcontext import wgl

    def create(*args, **kwargs):
        _apply_env_var(kwargs, 'glversion', 'GLCONTEXT_GLVERSION', arg_type=int)
        _apply_env_var(kwargs, 'libgl', 'GLCONTEXT_WIN_LIBGL')

        # make sure libgl is an absolute path
        if 'libgl' in kwargs:
            _libgl = kwargs['libgl']
            if '/' in _libgl or '\\' in _libgl:
                kwargs['libgl'] = os.path.abspath(_libgl)

        kwargs = _strip_kwargs(kwargs, ['glversion', 'mode', 'libgl'])
        return wgl.create_context(**kwargs)

    return create


def _x11():
    """Create x11 backend"""
    from glcontext import x11
    from ctypes.util import find_library

    def create(*args, **kwargs):
        if not kwargs.get('libgl'):
            kwargs['libgl'] = find_library('GL') 

        if not kwargs.get('libx11'):
            kwargs['libx11'] = find_library("X11")

        _apply_env_var(kwargs, 'glversion', 'GLCONTEXT_GLVERSION', arg_type=int)
        _apply_env_var(kwargs, 'libgl', 'GLCONTEXT_LINUX_LIBGL')
        _apply_env_var(kwargs, 'libx11', 'GLCONTEXT_LINUX_LIBX11')
        kwargs = _strip_kwargs(kwargs, ['glversion', 'mode', 'libgl', 'libx11'])
        return x11.create_context(**kwargs)

    return create


def _darwin():
    """Create darwin/cgl context"""
    from glcontext import darwin

    def create(*args, **kwargs):
        return darwin.create_context(**_strip_kwargs(kwargs, ['mode']))

    return create


def _egl():
    from glcontext import egl
    from ctypes.util import find_library

    def create(*args, **kwargs):
        if not kwargs.get('libgl'):
            kwargs['libgl'] = find_library('GL') 
        if not kwargs.get('libegl'):
            kwargs['libegl'] = find_library('EGL') 

        _apply_env_var(kwargs, 'device_index', 'GLCONTEXT_DEVICE_INDEX', arg_type=int)
        _apply_env_var(kwargs, 'glversion', 'GLCONTEXT_GLVERSION', arg_type=int)
        _apply_env_var(kwargs, 'libgl', 'GLCONTEXT_LINUX_LIBGL')
        _apply_env_var(kwargs, 'libegl', 'GLCONTEXT_LINUX_LIBEGL')
        kwargs = _strip_kwargs(kwargs, ['glversion', 'mode', 'libgl', 'libegl', 'device_index'])
        return egl.create_context(**kwargs)

    return create


def _strip_kwargs(kwargs: dict, supported_args: list):
    """Strips away unwanted keyword arguments.

    The backends are using ``PyArg_ParseTupleAndKeywords`` to
    parse the incoming ``kwargs`` data. It's not well suited
    to handle additional arguments.

        - Removes None key arguments
        - Removes unsupported arguments
    """
    return {k: v for k, v in kwargs.items() if v is not None and k in supported_args}


def _apply_env_var(kwargs, arg_name, env_name, arg_type=str):
    """Injects an environment variable into the arg dict if present"""
    value = os.environ.get(env_name, kwargs.get(arg_name))
    if value:
        kwargs[arg_name] = arg_type(value)

