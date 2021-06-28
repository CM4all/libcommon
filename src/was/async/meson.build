libwas_protocol = dependency('libcm4all-was-protocol')

was_async_sources = []
if get_option('coroutines')
  was_async_sources += [
    'CoHandler.cxx',
    'CoRun.cxx',
  ]
endif

was_async = static_library(
  'was_async',
  was_async_sources,
  'Control.cxx',
  'SimpleHandler.cxx',
  'SimpleInput.cxx',
  'SimpleOutput.cxx',
  'SimpleRun.cxx',
  'SimpleServer.cxx',
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
