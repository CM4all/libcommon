translation_server = static_library(
  'translation_server',
  'AllocatedRequest.cxx',
  'Response.cxx',
  'Connection.cxx',
  'Listener.cxx',
  'Server.cxx',
  include_directories: inc,
)

translation_server_dep = declare_dependency(
  link_with: translation_server,
  dependencies: [
    event_dep,
    event_net_dep,
  ],
)
