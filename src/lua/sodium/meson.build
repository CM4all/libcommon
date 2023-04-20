lua_sodium = static_library(
  'lua_sodium',
  'Init.cxx',
  'Box.cxx',
  include_directories: inc,
  dependencies: [
    lua_dep,
    sodium_dep,
  ],
)

lua_sodium_dep = declare_dependency(
  link_with: lua_sodium,
)
