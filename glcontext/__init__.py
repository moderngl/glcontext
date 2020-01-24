__version__ = '1.0.1'


def default_backend(standalone=False):
    """Get default backend based on the detected platform.
    Supports detecting an existing context and standalone contexts.
    If no context if found for the platform we return the linux backend.

    Example::

        # Get the available backend
        backend = get_default_backend(standalone=False/True)
        # Create an opengl 3.3 context or detect the currently active
        # context requiring at least opengl 3.3 support.
        ctx = backend(330)

    Args:
        standalone (bool): If ``False`` we detect an existing context.
                           If ``True`` we create a headless/standalone context.
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
        from glcontext import wgl
        mode = 'standalone' if standalone else 'detect'
        return lambda glversion: wgl.create_context(mode=mode, glversion=glversion)

    if target == 'linux':
        from glcontext import x11
        mode = 'standalone' if standalone else 'detect'
        return lambda glversion: x11.create_context(mode=mode, glversion=glversion)

    if target == 'darwin':
        from glcontext import darwin
        mode = 'standalone' if standalone else 'detect'
        return lambda glversion: darwin.create_context(mode=mode)
