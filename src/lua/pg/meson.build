lua_pg = static_library(
  'lua_pg',
  'Init.cxx',
  'Array.cxx',
  'Stock.cxx',
  'Result.cxx',
  include_directories: inc,
  dependencies: [
    lua_dep,
    pg_dep,
    stock_dep,
  ],
)

lua_pg_dep = declare_dependency(
  link_with: lua_pg,
)
