lua_net = static_library(
  'lua_net',
  'SocketAddress.cxx',
  include_directories: inc,
  dependencies: [
    lua_dep,
    net_dep,
  ],
)

lua_net_dep = declare_dependency(
  link_with: lua_net,
)
