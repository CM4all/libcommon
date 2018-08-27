libcares = dependency('libcares')

event_net_cares = static_library(
  'event_net_cares',
  'Error.cxx',
  'Init.cxx',
  'Channel.cxx',
  include_directories: inc,
  dependencies: [
    libcares,
  ],
)

event_net_cares_dep = declare_dependency(
  link_with: event_net_cares,
  dependencies: [
    event_dep,
    net_dep,
    util_dep,
  ],
)
