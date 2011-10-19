## Developer Guide

Dependencies:

- GIT
- GLib >=2.2
- Flex
- Bison
- GCC >= 4
- autotools

### Getting set up

#### Ubuntu / Debian

    $ apt-get install git flex bison automake autoconf libtool pkg-config libglib2.0-dev
    python-gobject-dev

#### FreeBSD
_Work in progress_

    $ pkg_add -r git flex bison autotools gmake pkg-config glib20 py26-gobject

### Building the source

    $ ./autogen.sh --enable-debug
    $ make

### Running the test suite

    $ make test

