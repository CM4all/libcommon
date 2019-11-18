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

translation_server = static_library(
  'translation_server',
  'AllocatedRequest.cxx',
  'Response.cxx',
  'Connection.cxx',
  'Listener.cxx',
  'Server.cxx',
  include_directories: inc,
  dependencies: [
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
