event_net = static_library(
  'event_net',
  'ConnectSocket.cxx',
  'ServerSocket.cxx',
  'UdpListener.cxx',
  'MultiUdpListener.cxx',
  'SocketWrapper.cxx',
  'BufferedSocket.cxx',
  'djb/NetstringServer.cxx',
  'djb/NetstringClient.cxx',
  'djb/QmqpClient.cxx',
  'log/PipeAdapter.cxx',
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
