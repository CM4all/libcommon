liblua = dependency('luajit')

lua_sources = []

if not is_variable('coroutines_dep')
  coroutines_dep = dependency('', required: false)
endif

if coroutines_dep.found()
  lua_sources += [
    'CoAwaitable.cxx',
  ]
endif

lua = static_library(
  'lua',
  'CoRunner.cxx',
  'Error.cxx',
  'Panic.cxx',
  'Resume.cxx',
  'RunFile.cxx',
  'State.cxx',
  lua_sources,
  include_directories: inc,
  dependencies: liblua,
)

lua_dep = declare_dependency(
  link_with: lua,
  dependencies: liblua,
)
