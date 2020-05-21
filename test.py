import glcontext

backend = glcontext.default_backend()
ctx = backend(mode='standalone', glversion=330)  #, libgl='libGL.so.1')
print(ctx)
print(ctx.load('glViewport'))
