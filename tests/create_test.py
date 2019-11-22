from unittest import TestCase
import glcontext


class ContextTestCase(TestCase):

    def test_create(self):
        """Basic context testing"""
        # Create a standalone context
        backend = glcontext.default_backend(standalone=True)
        ctx = backend(330)

        # Ensure methods are present
        self.assertTrue(callable(ctx.load))
        self.assertTrue(callable(ctx.release))
        self.assertTrue(callable(ctx.__enter__))
        self.assertTrue(callable(ctx.__exit__))

        # Enter and exit context
        with ctx:
            pass

        # Ensure method loading works
        ptr = ctx.load('glEnable')
        self.assertIsInstance(ptr, int)
        self.assertGreater(ptr, 0)

        # Load non-existent gl method
        ptr = ctx.load('bogus')
        self.assertIsInstance(ptr, int)
        self.assertEqual(ptr, 0)

    def test_mass_create(self):
        """Create and destroy a large quantity of contexts"""
        for i in range(1000):
            backend = glcontext.default_backend(standalone=True)
            ctx = backend(330)
            ctx.release()
