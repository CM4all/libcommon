if not lua_dep.found()
  lua_io_dep = dependency('', required: false)
  subdir_done()
endif

lua_io = static_library(
  'lua_io',
  'XattrTable.cxx',
  include_directories: inc,
  dependencies: [
    lua_dep,
    io_dep,
    fmt_dep,
  ],
)

lua_io_dep = declare_dependency(
  link_with: lua_io,
)
