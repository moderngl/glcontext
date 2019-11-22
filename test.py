import glcontext

ctx = glcontext.default_backend(standalone=True)(300)
print(ctx.load('glBegin'))
