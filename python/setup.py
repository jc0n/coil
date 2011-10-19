#!/usr/bin/env python

import os
import sys

try:
    import buildconf
except ImportError:
    print("Please run make first")
    raise SystemExit

from distutils.core import setup, Extension

classifiers = """\
Development Status :: 2 - Pre-Alpha
Intended Audience :: Developers
License :: OSI Approved :: BSD License
Operating System :: POSIX
Programming Language :: C
Programming Language :: Python
Topic :: Software Development :: Compilers
Topic :: Software Development :: Libraries
"""

libname = 'libcoil-%s' % buildconf.PACKAGE_VERSION

if os.system('pkg-config --exists ' + libname + ' >/dev/null') == 0:
    with os.popen('pkg-config --cflags ' + libname) as pkgcfg:
        cflags = pkgcfg.readline().strip().split()
    with os.popen('pkg-config --libs ' + libname) as pkgcfg:
        libs = pkgcfg.readline().strip().split()
else:
    print("Unable to read pkg-config for %s, build terminated" % libname)
    raise SystemExit


if os.system('pkg-config --exists pygobject-2.0 >/dev/null') == 0:
    with os.popen('pkg-config --cflags pygobject-2.0') as pkgcfg:
        cflags.extend(pkgcfg.readline().strip().split())
    with os.popen('pkg-config --libs pygobject-2.0') as pkgcfg:
        libs.extend((pkgcfg.readline().strip().split()))
else:
    print("Unable to read pkg-config for pygobject-2.0, build terminated")
    raise SystemExit

cflags = frozenset(cflags)
libs = frozenset(libs)

iflags = [x[2:] for x in cflags if x.startswith('-I')]
extra_cflags = [x for x in cflags if not x.startswith('-I')]

libdirs = [x[2:] for x in libs if x.startswith('-L')]
libsonly = [x[2:] for x in libs if x.startswith('-l')]

coilmodule = Extension('ccoil',
                       sources=['coilmodule.c',
                                'coilstruct.c',
                                'coillistproxy.c'],
                       include_dirs = iflags + ['..'],
                       extra_compile_args = extra_cflags,
                       library_dirs = libdirs,
                       libraries = libsonly)


if sys.version_info < (2, 3):
    _setup = setup
    def setup(**kw):
        if "classifiers" in kw:
            del kw["classifiers"]
        _setup(**kw)

if __name__ == '__main__':
    setup(name = 'ccoil',
          version=buildconf.PACKAGE_VERSION,
          description = 'Bindings for libcoil',
          classifiers = [x for x in classifiers.split("\n") if x],
          license = 'BSD',
          platforms = ['posix'],
          url = 'http://github.com/jc0n/coil',
          author = "John O'Connor",
          author_email = buildconf.PACKAGE_BUGREPORT,
          maintainer = "John O'Connor",
          maintainer_email = buildconf.PACKAGE_BUGREPORT,
          ext_modules = [coilmodule])
