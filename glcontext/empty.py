class GLContext:
    def __init__(self):
        pass

    def load_opengl_function(self, name):
        return 0

    def __enter__(self):
        pass

    def __exit__(self, *args):
        pass

    def release(self):
        pass


def create_context():
    return GLContext()
