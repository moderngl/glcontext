import glcontext.egl as backend

ctx = backend.create_context(mode='standalone')
print(ctx)
print(ctx.load('glGetIntegerv'))
