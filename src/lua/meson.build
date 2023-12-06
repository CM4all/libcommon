liblua = dependency('luajit',
                    required: get_variable('libcommon_require_lua', true))
if not liblua.found()
  lua_dep = liblua
  subdir_done()
endif

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
  'AutoCloseList.cxx',
  'CoCancel.cxx',
  'CoRunner.cxx',
  'Error.cxx',
  'ReloadRunner.cxx',
  'Resume.cxx',
  'RunFile.cxx',
  'State.cxx',
  lua_sources,
  include_directories: inc,
  dependencies: [
    coroutines_dep,
    liblua,
  ],
)

lua_dep = declare_dependency(
  link_with: lua,
  dependencies: [
    coroutines_dep,
    liblua,
  ],
)
