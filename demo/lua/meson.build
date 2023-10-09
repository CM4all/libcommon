if not lua_dep.found()
  subdir_done()
endif

executable(
  'RunLua',
  'RunLua.cxx',
  include_directories: inc,
  dependencies: [
    lua_dep,
    lua_json_dep,
    lua_mariadb_dep,
    util_dep,
  ],
)

if lua_pg_dep.found()
  executable(
    'CoLua',
    'CoLua.cxx',
    include_directories: inc,
    dependencies: [
      lua_dep,
      lua_event_dep,
      lua_pg_dep,
      event_dep,
      util_dep,
    ],
  )
endif
