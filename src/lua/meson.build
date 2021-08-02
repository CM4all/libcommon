liblua = dependency('luajit')

lua = static_library(
  'lua',
  'Error.cxx',
  'Panic.cxx',
  'RunFile.cxx',
  'State.cxx',
  include_directories: inc,
  dependencies: [
    liblua,
  ],
)

lua_dep = declare_dependency(
  link_with: lua,
  dependencies: liblua,
)
