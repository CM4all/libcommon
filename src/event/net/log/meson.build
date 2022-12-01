event_net_log = static_library(
  'event_net_log',
  'PipeAdapter.cxx',
  include_directories: inc,
  dependencies: [
    net_log_dep,
    event_dep,
  ],
)

event_net_log_dep = declare_dependency(
  link_with: event_net_log,
  dependencies: [
    net_log_dep,
    event_dep,
  ],
)
