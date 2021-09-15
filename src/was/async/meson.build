libwas_protocol = dependency('libcm4all-was-protocol', version: '>= 1.21')

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
if get_option('coroutines')
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
    event_net_dep,
  ],
)

was_server_async_dep = declare_dependency(
  link_with: was_server_async,
  dependencies: [
    was_async_dep,
  ],
)
