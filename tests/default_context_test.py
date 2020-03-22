import os
from unittest import TestCase
import psutil
import glcontext


class ContextTestCase(TestCase):

    def test_create(self):
        """Basic context testing"""
        # Create a standalone context
        # os.environ['GLCONTEXT_WIN_LIBGL'] = 'moo.dll'
        # os.environ['GLCONTEXT_LINUX_LIBGL'] = 'ligGL.so.1'
        # os.environ['GLCONTEXT_GLVERSION'] = '430'
        backend = glcontext.default_backend()
        ctx = backend(mode='standalone', glversion=330)

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
        # NOTE: Disabled for now since x11 returns positive values
        #       for non-existent methods
        # ptr = ctx.load('bogus')
        # self.assertIsInstance(ptr, int)
        # self.assertEqual(ptr, 0)

    def test_mass_create(self):
        """Create and destroy a large quantity of contexts.
        The rss memory usage should not grow more than 5x
        after allocating 1000 contexts.
        """
        process = psutil.Process(os.getpid())
        start_rss = process.memory_info().rss

        for i in range(1000):
            ctx = glcontext.default_backend()(mode='standalone', glversion=330)
            # Ensure we can enter context and load a method as a minimum
            with ctx:
                self.assertGreater(ctx.load('glBegin'), 0)
            ctx.release()

        end_rss = process.memory_info().rss
        self.assertTrue(end_rss / start_rss < 5.0)
