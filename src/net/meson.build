net_sources = [
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
  'ToString.cxx',
  'Interface.cxx',
  'SocketDescriptor.cxx',
  'UniqueSocketDescriptor.cxx',
  'SocketConfig.cxx',
  'RBindSocket.cxx',
  'RConnectSocket.cxx',
  'ConnectSocket.cxx',
  'MultiReceiveMessage.cxx',
  'SendMessage.cxx',
  'djb/NetstringInput.cxx',
  'djb/NetstringHeader.cxx',
  'djb/NetstringGenerator.cxx',
  'Buffered.cxx',
]

net_dependencies = [
  system_dep,
  io_dep,
]

if get_variable('libcommon_enable_net_log', true)
  net_sources += [
    'log/String.cxx',
    'log/Parser.cxx',
    'log/OneLine.cxx',
    'log/Send.cxx',
    'log/Serializer.cxx',
  ]
  net_dependencies += http_dep
endif

net = static_library(
  'net',
  net_sources,
  include_directories: inc,
  dependencies: [
  ],
)

net_dep = declare_dependency(
  link_with: net,
  dependencies: net_dependencies,
)
