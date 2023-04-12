test(
  'TestLua',
  executable(
    'TestLua',
    'TestCoAwaitable.cxx',
    include_directories: inc,
    dependencies: [
      gtest,
      lua_dep,
      lua_event_dep,
    ],
  ),
)

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
