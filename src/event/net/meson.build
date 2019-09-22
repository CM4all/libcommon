event_net_sources = [
  'ConnectSocket.cxx',
  'ServerSocket.cxx',
  'UdpHandler.cxx',
  'UdpListener.cxx',
  'MultiUdpListener.cxx',
  'SocketWrapper.cxx',
  'djb/NetstringServer.cxx',
  'djb/NetstringClient.cxx',
  'djb/QmqpClient.cxx',
  'log/PipeAdapter.cxx',
]

if get_variable('libcommon_enable_DefaultFifoBuffer', true)
  event_net_sources += [
    'BufferedSocket.cxx',
  ]
endif

event_net = static_library(
  'event_net',
  event_net_sources,
  include_directories: inc,
)

event_net_dep = declare_dependency(
  link_with: event_net,
  dependencies: [
    event_dep,
    net_dep,
    util_dep,
  ],
)
