net_sources = [
  'Anonymize.cxx',
  'BareInetAddress.cxx',
  'ChmodLocalSocket.cxx',
  'SocketError.cxx',
  'SocketAddress.cxx',
  'StaticSocketAddress.cxx',
  'AllocatedSocketAddress.cxx',
  'MaskedInetAddress.cxx',
  'IPv4Address.cxx',
  'IPv6Address.cxx',
  'LocalSocketAddress.cxx',
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
  'TempListener.cxx',
]

net_dependencies = [
  system_dep,
  io_dep,
]

if is_variable('io_config_dep')
  net_sources += [
    'SocketConfigParser.cxx',
  ]
  net_dependencies += io_config_dep
endif

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
