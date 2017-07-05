libcommon
=========

libcommon is a collection of reusable C++ code shared by many C++
projects at `Content Management AG <https://www.cm4all.com/>`__.

The project was created when code duplication between C++ projects got
out of hand.  Commonly used code was moved into this library, instead
of keeping several copies in sync.

The library is meant to be included in other projects as a git
submodule.  It is a lively and volatile project, which makes it hard
to turn it into a shared library.  Retaining ABI stability (which is
very difficult with C++ anyway) would be too hard at this stage, and
not worthwile.


Building libcommon
------------------

You need:

- a C++14 compiler (gcc 4.9 or clang)
- `meson 0.37 <http://mesonbuild.com/>`__
- `Boost <http://boost.org/>`__
- `libevent <http://libevent.org/>`__
- `libcurl <https://curl.haxx.se/>`__
- `libpq <https://www.postgresql.org/>`__
- `libsystemd <https://www.freedesktop.org/wiki/Software/systemd/>`__
- `libdbus <https://www.freedesktop.org/wiki/Software/dbus/>`__
- `libseccomp <https://github.com/seccomp/libseccomp>`__
- `OpenSSL <https://www.openssl.org/>`__
- `LuaJit <http://luajit.org/>`__

To build it, type::

  meson . build
  ninja -C build

That produces several static libraries.


Contents
--------

Each directory below ``src`` contains a sub-library:

- ``util``: generic utilities
- ``time``: dealing with date and time
- ``http``: HTTP protocol definitions and helpers
- ``adata``: data structures using our pool allocator
- ``system``: operating system utilities
- ``io``: file I/O utilities
- ``net``: networking/socket utilities
- ``event``: `libevent <http://libevent.org/>`__ C++ wrapper
- ``lua``: `Lua <http://www.lua.org/>`__ C++ wrappers
- ``curl``: `libcurl <https://curl.haxx.se/>`__ C++ wrappers with
  libevent integration
- ``pg``: `libpq <https://www.postgresql.org/>`__ C++ wrappers with
  libevent integration
- ``odbus``: `libdbus
  <https://www.freedesktop.org/wiki/Software/dbus/>`__ C++ wrappers
- ``translation``: implementation of the CM4all translation protocol
- ``spawn``: a process spawner
