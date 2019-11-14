__version__ = '1.0.0'


def default_backend(standalone=False):
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
