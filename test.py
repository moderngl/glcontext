import glcontext

backend = glcontext.default_backend(standalone=True)
print(backend)
ctx = backend(330)
print(ctx.load)
print(ctx.release)
print(ctx.__enter__)
print(ctx.__exit__)
print(ctx.load('glEnable'))
print(ctx.load('glScissor'))
print(ctx.load('bogus'))
