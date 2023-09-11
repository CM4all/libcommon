if not lua_dep.found() or not pg_dep.found()
  lua_pg_dep = dependency('', required: false)
  subdir_done()
endif

lua_pg = static_library(
  'lua_pg',
  'Init.cxx',
  'Array.cxx',
  'Connection.cxx',
  'Result.cxx',
  include_directories: inc,
  dependencies: [
    lua_dep,
    pg_dep,
    fmt_dep,
  ],
)

lua_pg_dep = declare_dependency(
  link_with: lua_pg,
)
