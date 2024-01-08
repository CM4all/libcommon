net_sources = [
  'Anonymize.cxx',
  'SocketError.cxx',
  'SocketAddress.cxx',
  'StaticSocketAddress.cxx',
  'AllocatedSocketAddress.cxx',
  'MaskedSocketAddress.cxx',
  'IPv4Address.cxx',
  'IPv6Address.cxx',
  'HostParser.cxx',
  'AddressInfo.cxx',
  'Resolver.cxx',
  'Parser.cxx',
  'FormatAddress.cxx',
  'ToString.cxx',
  'Interface.cxx',
  'SocketDescriptor.cxx',
  'UniqueSocketDescriptor.cxx',
  'SocketPair.cxx',
  'SocketConfig.cxx',
  'BindSocket.cxx',
  'RBindSocket.cxx',
  'RConnectSocket.cxx',
  'ConnectSocket.cxx',
  'MultiReceiveMessage.cxx',
  'SendMessage.cxx',
  'EasyMessage.cxx',
  'Buffered.cxx',
]

net_dependencies = [
  system_dep,
  io_dep,
]

net = static_library(
  'net',
  net_sources,
  include_directories: inc,
  dependencies: [
    fmt_dep,
  ],
)

net_dep = declare_dependency(
  link_with: net,
  dependencies: net_dependencies,
)
