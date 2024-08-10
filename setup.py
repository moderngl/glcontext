import platform
import sys

from setuptools import Extension, setup

PLATFORMS = {'windows', 'linux', 'darwin'}

target = platform.system().lower()

for known in PLATFORMS:
    if target.startswith(known):
        target = known

if target not in PLATFORMS:
    target = 'linux'

wgl = Extension(
    name='glcontext.wgl',
    sources=['glcontext/wgl.cpp'],
    extra_compile_args=['-fpermissive'] if 'GCC' in sys.version else [],
    libraries=['user32', 'gdi32'],
)

x11 = Extension(
    name='glcontext.x11',
    sources=['glcontext/x11.cpp'],
    extra_compile_args=['-fpermissive'],
    libraries=['dl'],
)

egl = Extension(
    name='glcontext.egl',
    sources=['glcontext/egl.cpp'],
    extra_compile_args=['-fpermissive'],
    libraries=['dl'],
)

headless = Extension(
    name='glcontext.headless',
    sources=['glcontext/headless.cpp'],
    libraries=['EGL'],
)

if target == 'windows':
    windowed = Extension(
        name='glcontext.windowed',
        sources=['glcontext/windowed.cpp'],
        libraries=['opengl32'],
    )

if target == 'linux':
    windowed = Extension(
        name='glcontext.windowed',
        sources=['glcontext/windowed.cpp'],
        extra_compile_args=['-fpermissive'],
        libraries=['GL'],
    )

if target == 'darwin':
    windowed = Extension(
        name='glcontext.windowed',
        sources=['glcontext/windowed.cpp'],
        extra_compile_args=['-fpermissive', '-Wno-deprecated-declarations'],
        extra_link_args=['-framework', 'OpenGL', '-Wno-deprecated'],
    )

darwin = Extension(
    name='glcontext.darwin',
    sources=['glcontext/darwin.cpp'],
    extra_compile_args=['-fpermissive', '-Wno-deprecated-declarations'],
    extra_link_args=['-framework', 'OpenGL', '-Wno-deprecated'],
)

ext_modules = {
    'windows': [wgl],
    'linux': [x11, egl],
    'darwin': [darwin],
}

setup(
    name='glcontext',
    version='3.0.0',
    description='Portable Headless OpenGL Context',
    long_description=open('README.md', encoding='utf-8').read(),
    long_description_content_type='text/markdown',
    url='https://github.com/moderngl/glcontext',
    author='Szabolcs Dombi',
    author_email='szabolcs@szabolcsdombi.com',
    license='MIT',
    platforms=['any'],
    packages=['glcontext'],
    ext_modules=ext_modules[target],
    classifiers=[
        'Development Status :: 5 - Production/Stable',
        'License :: OSI Approved :: MIT License',
        'Operating System :: OS Independent',
        'Topic :: Games/Entertainment',
        'Topic :: Multimedia :: Graphics',
        'Topic :: Multimedia :: Graphics :: 3D Rendering',
        'Topic :: Scientific/Engineering :: Visualization',
        'Programming Language :: Python :: 3 :: Only',
    ],
)
