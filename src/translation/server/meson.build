translation_server_sources = []

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

if get_option('coroutines')
  translation_server_sources += 'CoHandler.cxx'
endif

translation_server = static_library(
  'translation_server',
  'AllocatedRequest.cxx',
  'Response.cxx',
  'Connection.cxx',
  'Listener.cxx',
  'Server.cxx',
  'FunctionHandler.cxx',
  translation_server_sources,
  include_directories: inc,
  dependencies: [
    event_dep,
    libsystemd,
  ],
)

translation_server_dep = declare_dependency(
  link_with: translation_server,
  dependencies: [
    event_dep,
    event_net_dep,
  ],
)
