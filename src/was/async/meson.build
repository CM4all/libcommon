libwas_protocol = dependency('libcm4all-was-protocol', version: '>= 1.24', required: get_variable('libcommon_enable_was', true))
if not libwas_protocol.found()
  was_async_dep = disabler()
  was_server_async_dep = disabler()
  subdir_done()
endif

if get_variable('libcommon_enable_libsystemd', true)
  libsystemd = dependency('libsystemd')
  libsystemd = declare_dependency(
    compile_args: ['-DHAVE_LIBSYSTEMD'],
    dependencies: [
      libsystemd,
    ],
  )
else
  libsystemd = dependency('', required: false)
endif

was_async = static_library(
  'was_async',
  'Control.cxx',
  'SimpleInput.cxx',
  'SimpleOutput.cxx',
  'MultiClient.cxx',
  'Socket.cxx',
  include_directories: inc,
  dependencies: [
    libwas_protocol,
    event_net_dep,
  ],
)

was_async_dep = declare_dependency(
  link_with: was_async,
  dependencies: [
    libwas_protocol,
  ],
)

was_server_async_sources = []

if not is_variable('coroutines_dep')
  coroutines_dep = dependency('', required: false)
endif

if coroutines_dep.found()
  was_server_async_sources += [
    'CoHandler.cxx',
    'CoRun.cxx',
  ]
endif

was_server_async = static_library(
  'was_server_async',
  was_server_async_sources,
  'SimpleHandler.cxx',
  'SimpleRun.cxx',
  'SimpleServer.cxx',
  'SimpleMultiServer.cxx',
  'MultiClient.cxx',
  include_directories: inc,
  dependencies: [
    was_async_dep,
    libwas_protocol,
    libsystemd,
    event_net_dep,
    coroutines_dep,
  ],
)

was_server_async_dep = declare_dependency(
  link_with: was_server_async,
  dependencies: [
    was_async_dep,
    coroutines_dep,
  ],
)
