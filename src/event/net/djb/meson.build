event_net_djb = static_library(
  'event_net_djb',
  'NetstringServer.cxx',
  'NetstringClient.cxx',
  'QmqpClient.cxx',
  include_directories: inc,
  dependencies: [
    net_djb_dep,
    event_dep,
  ],
)

event_net_djb_dep = declare_dependency(
  link_with: event_net_djb,
  dependencies: [
    event_dep,
  ],
)
