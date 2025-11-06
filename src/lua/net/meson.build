lua_net_sources = []
lua_net_dependencies = []

if is_variable('control_client_dep')
  lua_net_sources += 'ControlClient.cxx'
  lua_net_dependencies += control_client_dep
endif

lua_net = static_library(
  'lua_net',
  'Socket.cxx',
  'SocketAddress.cxx',
  'Resolver.cxx',
  lua_net_sources,
  include_directories: inc,
  dependencies: [
    lua_dep,
    net_dep,
    lua_net_dependencies,
  ],
)

lua_net_dep = declare_dependency(
  link_with: lua_net,
)
