lua_mariadb = static_library(
  'lua_mariadb',
  'Init.cxx',
  'Connection.cxx',
  'SResult.cxx',
  include_directories: inc,
  dependencies: [
    lua_dep,
    mariadb_dep,
  ],
)

lua_mariadb_dep = declare_dependency(
  link_with: lua_mariadb,
)
