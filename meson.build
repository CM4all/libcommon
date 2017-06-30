project('CM4all libcommon', ['c', 'cpp'], version: '0.1',
  default_options: [
    'c_std=c99',
    'cpp_std=c++14'
  ],
)

compiler = meson.get_compiler('cpp')

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

boost = dependency('boost', modules: ['system', 'filesystem'], version: '>= 1.54')
libevent = dependency('libevent', version: '>= 2.0.19')
libcurl = dependency('libcurl', version: '>= 7.38')
libpq = dependency('libpq', version: '>= 8.4')
libsystemd = dependency('libsystemd')
libdbus = dependency('dbus-1')
libseccomp = dependency('libseccomp')
liblua = dependency('luajit')
libdaemon = dependency('libcm4all-daemon', version: '>= 0.4.1')
libbp = dependency('libcm4all-beng-proxy', version: '>= 12.0.22', required: false)

gtest_warning_flags = []
if compiler.get_id() == 'gcc'
  gtest_warning_flags += [
    '-Wno-suggest-attribute=format',
    '-Wno-suggest-attribute=noreturn',
  ]
endif

gtest = declare_dependency(
  dependencies: [dependency('gtest', main: true)],
  compile_args: gtest_warning_flags + [
    '-Wno-undef',
    '-Wno-missing-declarations',

    # needed on Jessie for gtest's IsNullLiteralHelper
    '-Wno-conversion-null',
  ],
)

inc = include_directories('src', 'fake')

util = static_library('util',
  'src/util/AllocatedString.cxx',
  'src/util/Exception.cxx',
  'src/util/LeakDetector.cxx',
  'src/util/PrintException.cxx',
  'src/util/StringBuilder.cxx',
  'src/util/StringCompare.cxx',
  'src/util/StringParser.cxx',
  'src/util/StringUtil.cxx',
  'src/util/StringView.cxx',
  'src/util/HexFormat.c',
  'src/util/djbhash.c',
  include_directories: inc,
  dependencies: [
  ])
util_dep = declare_dependency(link_with: util)

time = static_library('time',
  'src/time/gmtime.c',
  include_directories: inc,
  dependencies: [
  ])
time_dep = declare_dependency(link_with: time)

http = static_library('http',
  'src/http/List.cxx',
  'src/http/Date.cxx',
  'src/http/Range.cxx',
  include_directories: inc,
  dependencies: [
    util_dep,
  ])
http_dep = declare_dependency(link_with: http)

adata = static_library('adata',
  'src/adata/ExpandableStringList.cxx',
  include_directories: inc,
  dependencies: [
  ])
adata_dep = declare_dependency(link_with: adata)

lua = static_library('lua',
  'src/lua/Error.cxx',
  'src/lua/Panic.cxx',
  'src/lua/RunFile.cxx',
  'src/lua/State.cxx',
  include_directories: inc,
  dependencies: [
    liblua,
  ]
)
lua_dep = declare_dependency(link_with: lua)

event = static_library('event',
  'src/event/Loop.cxx',
  'src/event/ShutdownListener.cxx',
  'src/event/CleanupTimer.cxx',
  'src/event/DeferEvent.cxx',
  'src/event/SignalEvent.cxx',
  'src/event/SocketWrapper.cxx',
  'src/event/PipeLineReader.cxx',
  include_directories: inc,
  dependencies: [
    libevent,
    util_dep,
  ])
event_dep = declare_dependency(link_with: event)

io = static_library('io',
  'src/io/FileDescriptor.cxx',
  'src/io/WriteFile.cxx',
  'src/io/WriteBuffer.cxx',
  'src/io/MultiWriteBuffer.cxx',
  'src/io/FileWriter.cxx',
  'src/io/LineParser.cxx',
  'src/io/FileLineParser.cxx',
  'src/io/ConfigParser.cxx',
  include_directories: inc,
  dependencies: [
  ])
io_dep = declare_dependency(link_with: io)

system = static_library('system',
  'src/system/BindMount.cxx',
  'src/system/ProcessName.cxx',
  include_directories: inc,
  dependencies: [
    util_dep,
    io_dep,
  ])
system_dep = declare_dependency(link_with: system)

net = static_library('net',
  'src/net/SocketAddress.cxx',
  'src/net/StaticSocketAddress.cxx',
  'src/net/AllocatedSocketAddress.cxx',
  'src/net/HostParser.cxx',
  'src/net/Resolver.cxx',
  'src/net/Parser.cxx',
  'src/net/Interface.cxx',
  'src/net/SocketDescriptor.cxx',
  'src/net/UniqueSocketDescriptor.cxx',
  'src/net/RConnectSocket.cxx',
  'src/net/ConnectSocket.cxx',
  'src/net/ServerSocket.cxx',
  'src/net/djb/NetstringServer.cxx',
  'src/net/djb/NetstringClient.cxx',
  'src/net/djb/NetstringInput.cxx',
  'src/net/djb/NetstringHeader.cxx',
  'src/net/djb/NetstringGenerator.cxx',
  'src/net/djb/QmqpClient.cxx',
  'src/net/Buffered.cxx',
  include_directories: inc,
  dependencies: [
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
    event_dep,
  ])

pg = static_library('pg',
  'src/pg/Serial.cxx',
  'src/pg/Array.cxx',
  'src/pg/Connection.cxx',
  'src/pg/AsyncConnection.cxx',
  'src/pg/Result.cxx',
  'src/pg/Reflection.cxx',
  include_directories: inc,
  dependencies: [
    libpq,
  ])
pg_dep = declare_dependency(link_with: pg)

odbus = static_library('odbus',
  'src/odbus/Message.cxx',
  include_directories: inc,
  dependencies: [
    libdbus,
  ])
odbus_dep = declare_dependency(link_with: odbus)

spawn = static_library('spawn',
  'src/spawn/SeccompFilter.cxx',
  'src/spawn/SyscallFilter.cxx',
  'src/spawn/Systemd.cxx',
  'src/spawn/Prepared.cxx',
  'src/spawn/Registry.cxx',
  'src/spawn/Init.cxx',
  'src/spawn/Direct.cxx',
  'src/spawn/Interface.cxx',
  'src/spawn/Local.cxx',
  'src/spawn/NamespaceOptions.cxx',
  'src/spawn/MountList.cxx',
  'src/spawn/JailConfig.cxx',
  'src/spawn/JailParams.cxx',
  'src/spawn/ChildOptions.cxx',
  'src/spawn/CgroupOptions.cxx',
  'src/spawn/UidGid.cxx',
  'src/spawn/ResourceLimits.cxx',
  'src/spawn/RefenceOptions.cxx',
  'src/spawn/Server.cxx',
  'src/spawn/Launch.cxx',
  'src/spawn/Client.cxx',
  'src/spawn/Glue.cxx',
  'src/spawn/ConfigParser.cxx',
  include_directories: inc,
  dependencies: [
    libdbus,
    libseccomp,
    libsystemd,
    libdaemon,
    adata_dep,
    io_dep,
    odbus_dep,
  ])
spawn_dep = declare_dependency(link_with: spawn)

if libbp.found()
  translation = static_library('translation',
    'src/translation/PReader.cxx',
    'src/translation/Parser.cxx',
    'src/translation/Response.cxx',
    include_directories: inc,
    dependencies: [
      libbp,
      declare_dependency(link_with: event),
      declare_dependency(link_with: net),
    ])
endif

subdir('test')
