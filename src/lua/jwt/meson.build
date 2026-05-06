lua_jwt = static_library(
  'lua_jwt',
  'Init.cxx',
  include_directories: inc,
  dependencies: [
    lua_dep,
    jwt_dep,
  ],
)

lua_jwt_dep = declare_dependency(
  link_with: lua_jwt,
)
