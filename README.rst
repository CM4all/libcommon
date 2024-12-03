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
- `Meson 1.0 <http://mesonbuild.com/>`__
- `libfmt <https://fmt.dev/>`__
- `libsystemd <https://www.freedesktop.org/wiki/Software/systemd/>`__
- `libdbus <https://www.freedesktop.org/wiki/Software/dbus/>`__
- `libcap <https://sites.google.com/site/fullycapable/>`__
- `libseccomp <https://github.com/seccomp/libseccomp>`__
- `OpenSSL <https://www.openssl.org/>`__

Optional dependencies:

- `Avahi <https://www.avahi.org/>`__
- `libcurl <https://curl.haxx.se/>`__
- `libmariadb <https://mariadb.org/>`__
- `libpq <https://www.postgresql.org/>`__
- `libsodium <https://www.libsodium.org/>`__
- `liburing <https://github.com/axboe/liburing>`__
- `libwas <https://github.com/CM4all/libwas>`__
- `LuaJit <http://luajit.org/>`__
- `Nettle <https://www.lysator.liu.se/~nisse/nettle/>`__
- `nlohmann_json <https://json.nlohmann.me/>`__
- `PCRE <https://www.pcre.org/>`__
- `GoogleTest <https://github.com/google/googletest>`__

To build it, type::

  meson . build
  ninja -C build

That produces several static libraries.


Contents
--------

Each directory below ``src`` contains a sub-library:

- ``adata``: data structures using our pool allocator
- ``co``: C++20 Coroutines
- ``event``: a non-blocking I/O event loop
- ``event/co``: integration of C++20 Coroutines into our event loop
- ``event/net``: non-blocking networking libraries
- ``event/systemd``: non-blocking systemd clients
- ``event/uring``: integration of io_uring in the event loop
- ``http``: HTTP protocol definitions and helpers
- ``io``: file I/O utilities
- ``io/config``: a configuration file parser
- ``io/linux``: Linux-specific I/O helpers
- ``io/uring``: `liburing <https://github.com/axboe/liburing>`__ C++
  wrapper
- ``jwt``: helpers for JSON Web Tokens
- ``lib``: C++ wrappers or additional utilities for various
  external libraries.
- ``lua``: `Lua <http://www.lua.org/>`__ C++ wrappers
- ``lua/event``: non-blocking Lua
- ``lua/io``: I/O helpers for Lua
- ``lua/json``: Lua JSON library
- ``lua/mariadb``: Lua wrapper for libmariadb
- ``lua/net``: networking helpers for Lua
- ``lua/pg``: non-blocking PostgreSQL client for Lua
- ``lua/sodium``: Lua wrappers for `libsodium
  <https://libsodium.org/>`__
- ``memory``: memory allocators
- ``net``: networking/socket utilities
- ``net/control``: the control protocol (a datagram-based protocol to
  control several of our daemons)
- ``net/djb``: implementations of protocols designed by
  `D. J. Bernstein <https://cr.yp.to/>`__
- ``net/linux``: Linux-specific networking utilities
- ``net/log``: our multicast-based logging protocol
- ``pg``: `libpq <https://www.postgresql.org/>`__ C++ wrappers
- ``spawn``: a process spawner
- ``stock``: manage stocks of reusable objects (e.g. for connection
  pooling)
- ``system``: operating system utilities
- ``system/linux``: Linux-specific utilities, e.g. system call wrappers
- ``thread``: helpers for multi-threaded applications
- ``time``: dealing with date and time
- ``translation``: implementation of the CM4all translation protocol
- ``translation/server``: non-blocking translation server framework
- ``uri``: URI utilities
- ``util``: generic utilities
- ``was``: helpers for `libwas <https://github.com/CM4all/libwas>`__
- ``was/async``: a non-blocking implementation of the Web Application
  Socket protocol

These directories contain C++ wrappers or additional utilities for
external libraries:

- ``lib/avahi``: `Avahi <https://avahi.org/>`__ C++ wrappers
- ``lib/cap``: `libcap
  <https://sites.google.com/site/fullycapable/>`__ C++ wrappers
- ``lib/curl``: `libcurl <https://curl.haxx.se/>`__ C++ wrappers
- ``lib/dbus``: `libdbus
  <https://www.freedesktop.org/wiki/Software/dbus/>`__ C++ wrappers
- ``lib/fmt``: `libfmt <https://fmt.dev/>`__ helpers
- ``lib/mariadb``: C++ wrappers for the `MariaDB
  <https://mariadb.org/>`__ client library
- ``lib/nettle``: `Nettle
  <https://www.lysator.liu.se/~nisse/nettle/>`__ C++ wrappers
- ``lib/nlohmann_json``: `nlohmann_json
  <https://json.nlohmann.me/>`__ helpers
- ``lib/openssl``: `OpenSSL <https://www.openssl.org/>`__ C++ wrappers
- ``lib/pcre``: `PCRE <https://www.pcre.org/>`__ C++ wrappers
- ``lib/sodium``: `libsodium <https://github.com/jedisct1/libsodium/>`__
  C++ wrappers
- ``lib/zlib``: `zlib <https://zlib.net//>`__ C++ wrappers

Special directories:

- ``pluggable``: contains fallback implementations for modules that
  should be replaced by applications using libcommon
