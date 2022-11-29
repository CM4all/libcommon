libcommon
=========

libcommon is a collection of reusable C++ code shared by many C++
projects at `CM4all GmbH <https://www.cm4all.com/>`__.

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

- a C++20 compiler (`GCC <https://gcc.gnu.org/>`__ or `clang
  <https://clang.llvm.org/>`__)
- `meson 0.56 <http://mesonbuild.com/>`__
- `Boost <http://boost.org/>`__
- `libcurl <https://curl.haxx.se/>`__
- `libpq <https://www.postgresql.org/>`__
- `libsystemd <https://www.freedesktop.org/wiki/Software/systemd/>`__
- `libdbus <https://www.freedesktop.org/wiki/Software/dbus/>`__
- `libcap <https://sites.google.com/site/fullycapable/>`__
- `libseccomp <https://github.com/seccomp/libseccomp>`__
- `liburing <https://github.com/axboe/liburing>`__
- `libwas <https://github.com/CM4all/libwas>`__
- `OpenSSL <https://www.openssl.org/>`__
- `LuaJit <http://luajit.org/>`__
- `PCRE <https://www.pcre.org/>`__

To build it, type::

  meson . build
  ninja -C build

That produces several static libraries.


Contents
--------

Each directory below ``src`` contains a sub-library:

- ``util``: generic utilities
- ``co``: C++20 Coroutines
- ``time``: dealing with date and time
- ``http``: HTTP protocol definitions and helpers
- ``adata``: data structures using our pool allocator
- ``system``: operating system utilities
- ``io``: file I/O utilities
- ``io/uring``: `liburing <https://github.com/axboe/liburing>`__ C++
  wrapper
- ``json``: standalone ``boost::json``
- ``net``: networking/socket utilities
- ``event``: a non-blocking I/O event loop
- ``lib``: C++ wrappers or additional utilities for various
  external libraries.
- ``lua``: `Lua <http://www.lua.org/>`__ C++ wrappers
- ``pg``: `libpq <https://www.postgresql.org/>`__ C++ wrappers
- ``translation``: implementation of the CM4all translation protocol
- ``spawn``: a process spawner
- ``stock``: manage stocks of reusable objects (e.g. for connection
  pooling)

These directories contain C++ wrappers or additional utilities for
external libraries:

- ``lib/curl``: `libcurl <https://curl.haxx.se/>`__ C++ wrappers
- ``lib/dbus``: `libdbus
  <https://www.freedesktop.org/wiki/Software/dbus/>`__ C++ wrappers
- ``lib/mariadb``: C++ wrappers for the `MariaDB
  <https://mariadb.org/>`__ client library
- ``lib/openssl``: `OpenSSL <https://www.openssl.org/>`__ C++ wrappers
- ``lib/pcre``: `PCRE <https://www.pcre.org/>`__ C++ wrappers
- ``lib/sodium``: `libsodium <https://github.com/jedisct1/libsodium/>`__
  C++ wrappers
- ``lib/zlib``: `zlib <https://zlib.net//>`__ C++ wrappers
