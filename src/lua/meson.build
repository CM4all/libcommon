liblua = dependency('luajit')

lua_sources = [
  'CoRunner.cxx',
  'Error.cxx',
  'Panic.cxx',
  'Resume.cxx',
  'RunFile.cxx',
  'State.cxx',
]

lua_dependencies = [liblua]

if get_variable('libcommon_enable_json', false)
  lua_sources += 'Json.cxx'
  lua_dependencies += boost_json_dep
endif

lua = static_library(
  'lua',
  lua_sources,
  include_directories: inc,
  dependencies: lua_dependencies,
)

lua_dep = declare_dependency(
  link_with: lua,
  dependencies: liblua,
)
