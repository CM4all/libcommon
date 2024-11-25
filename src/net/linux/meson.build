net_linux = static_library(
  'io',
  'PeerAuth.cxx',
  include_directories: inc,
  dependencies: [
    net_dep,
    io_linux_dep,
  ],
)

net_linux_dep = declare_dependency(
  link_with: net_linux,
  dependencies: [
    io_dep,
  ],
)
