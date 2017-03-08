project('CM4all libcommon', 'cpp', version: '0.1',
  default_options : ['cpp_std=c++14'])

add_global_arguments('-D_REENTRANT', '-D_GNU_SOURCE',
  '-DPACKAGE="' + meson.project_name() + '"',
  '-DVERSION="' + meson.project_version() + '"',
  '-Wall',
  '-Wextra',
  '-Wwrite-strings', '-Wcast-qual', '-Wcast-align', '-Wfloat-equal',
  '-Wshadow', '-Wpointer-arith', '-Wsign-compare',
  '-Wmissing-declarations', '-Wmissing-noreturn', '-Wmissing-format-attribute',
  '-Wredundant-decls', '-Wno-long-long', '-Wundef',
  '-Wunused',
  '-Wno-non-virtual-dtor',
  '-Werror',
  language: 'cpp')

boost = dependency('boost', version: '>= 1.54')
libevent = dependency('libevent', version: '>= 2.0.19')
libcurl = dependency('libcurl', version: '>= 7.38')
libpq = dependency('libpq', version: '>= 8.4')
libsystemd = dependency('libsystemd')
libinline = dependency('libcm4all-inline', version: '>= 0.3.4')
libsocket = dependency('libcm4all-socket', version: '>= 0.3.5')

inc = include_directories('src')

util = static_library('util',
  'src/util/AllocatedString.cxx',
  'src/util/Exception.cxx',
  'src/util/LeakDetector.cxx',
  'src/util/PrintException.cxx',
  'src/util/StringBuilder.cxx',
  'src/util/StringParser.cxx',
  'src/util/StringUtil.cxx',
  'src/util/StringView.cxx',
  include_directories: inc,
  dependencies: [
    libinline,
  ])
util_dep = declare_dependency(link_with: util)

event = static_library('event',
  'src/event/Loop.cxx',
  'src/event/ShutdownListener.cxx',
  'src/event/CleanupTimer.cxx',
  'src/event/DeferEvent.cxx',
  'src/event/SignalEvent.cxx',
  include_directories: inc,
  dependencies: [
    libinline,
    libevent,
    util_dep,
  ])
event_dep = declare_dependency(link_with: event)

net = static_library('net',
  'src/net/SocketAddress.cxx',
  'src/net/StaticSocketAddress.cxx',
  'src/net/AllocatedSocketAddress.cxx',
  'src/net/Resolver.cxx',
  'src/net/Parser.cxx',
  'src/net/Interface.cxx',
  'src/net/SocketDescriptor.cxx',
  include_directories: inc,
  dependencies: [
    libinline, libsocket,
  ])
net_dep = declare_dependency(link_with: net)

curl = static_library('curl',
  'src/curl/Version.cxx',
  'src/curl/Request.cxx',
  'src/curl/Global.cxx',
  'src/curl/Init.cxx',
  include_directories: inc,
  dependencies: [
    libcurl,
    libinline,
    event_dep,
  ])

pg = static_library('pg',
  'src/pg/Array.cxx',
  'src/pg/Connection.cxx',
  'src/pg/AsyncConnection.cxx',
  'src/pg/Result.cxx',
  include_directories: inc,
  dependencies: [
    libpq,
    libinline,
  ])
pg_dep = declare_dependency(link_with: pg)

subdir('test')
