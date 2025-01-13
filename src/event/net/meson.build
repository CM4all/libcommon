event_net_sources = [
  'ConnectSocket.cxx',
  'ServerSocket.cxx',
  'UdpListener.cxx',
  'MultiUdpListener.cxx',
  'SocketWrapper.cxx',
  'PingClient.cxx',
  'PrometheusExporterListener.cxx',
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
  dependencies: [
    fmt_dep,
    get_variable('uring_dep', dependency('', required: false)),
  ],
)

event_net_dep = declare_dependency(
  link_with: event_net,
  dependencies: [
    event_dep,
    net_dep,
    util_dep,
  ],
)
