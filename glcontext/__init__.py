__version__ = '2.0.0'


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
        supported_args = ['glversion', 'mode', 'libgl']
        return wgl.create_context(**_strip_kwargs(kwargs, supported_args))

    return create


def _x11():
    """Create x11 backend"""
    from glcontext import x11
    def create(*args, **kwargs):
        supported_args = ['glversion', 'mode', 'libgl', 'libx11']
        return x11.create_context(**_strip_kwargs(kwargs, supported_args))
    return create

def _darwin():
    """Create darwin/cgl context"""
    from glcontext import darwin
    def create(*args, **kwargs):
        supported_args = ['mode']
        return darwin.create_context(**_strip_kwargs(kwargs, supported_args))
    return create

def _egl():
    from glcontext import egl
    def create(*args, **kwargs):
        supported_args = ['glversion', 'mode', 'libgl', 'libegl']
        return egl.create_context(**_strip_kwargs(kwargs, supported_args))
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
