"""Error path and cleanup tests.

Every scenario runs in its own interpreter: a crash in the native code fails one test
instead of killing the pytest process. The scenarios are plain functions in this file,
run by ``python error_paths_test.py <scenario>``.
"""
import os
import subprocess
import sys
from unittest import TestCase

import pytest

SKIP = 77  # exit code of a scenario that cannot run on this machine

LINUX = sys.platform.startswith('linux')


# Scenarios, these run in the subprocess

def skip(reason):
    print('SKIP: %s' % reason)
    sys.exit(SKIP)


def backend_for(name):
    import glcontext
    try:
        if name == 'egl':
            backend = glcontext.get_backend_by_name('egl')
        else:
            backend = glcontext.default_backend()
        backend(mode='standalone', glversion=330).release()
    except Exception as e:
        skip('%s backend is not available: %s' % (name, e))
    return backend


def scenario_load_type_error(name):
    """load() with something that is not a str raises, it used to look up a NULL name and crash."""
    backend = backend_for(name)
    ctx = backend(mode='standalone', glversion=330)
    for method in (ctx.load, ctx.load_opengl_function):
        for arg in (123, None, b'glEnable', 1.5, object()):
            try:
                method(arg)
            except TypeError:
                pass
            else:
                raise AssertionError('load(%r) did not raise TypeError' % (arg,))
        try:
            method('\ud800')  # not encodable as UTF-8
        except UnicodeEncodeError:
            pass
        else:
            raise AssertionError('load of a lone surrogate did not raise UnicodeEncodeError')
        assert method('glEnable') > 0  # still works after the errors
    ctx.release()


SCENARIOS = {
    'egl_load_type_error': lambda: scenario_load_type_error('egl'),
    'x11_load_type_error': lambda: scenario_load_type_error('x11'),
}


# Tests, these run in pytest

def run_scenario(name):
    result = subprocess.run(
        [sys.executable, os.path.abspath(__file__), name],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        universal_newlines=True,
        timeout=60,
    )
    if result.returncode == SKIP:
        pytest.skip(result.stdout.strip())
    assert result.returncode == 0, 'scenario %s exited with %s\n%s' % (name, result.returncode, result.stdout)


class ErrorPathsTestCase(TestCase):

    @pytest.mark.skipif(not LINUX, reason='requires egl')
    def test_egl_load_type_error(self):
        run_scenario('egl_load_type_error')

    @pytest.mark.skipif(not LINUX, reason='requires x11')
    def test_x11_load_type_error(self):
        run_scenario('x11_load_type_error')


if __name__ == '__main__':
    SCENARIOS[sys.argv[1]]()
