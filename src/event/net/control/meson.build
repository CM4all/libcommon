control_server = static_library(
  'control_server',
  'Server.cxx',
  include_directories: inc,
  dependencies: [
    fmt_dep,
    event_net_dep,
  ],
)

control_server_dep = declare_dependency(
  link_with: control_server,
  dependencies: [
    event_net_dep,
  ],
)
