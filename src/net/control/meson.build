control_client = static_library(
  'control_client',
  'Client.cxx',
  include_directories: inc,
)

control_client_dep = declare_dependency(
  link_with: control_client,
  dependencies: [
    net_dep,
  ],
)
