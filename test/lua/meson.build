if not lua_dep.found()
  subdir_done()
endif

test_lua_sources = [
  'TestClass.cxx',
  'TestChrono.cxx',
  'TestCoroutine.cxx',
  'TestCoRunner.cxx',
  'TestError.cxx',
  'TestForEach.cxx',
]

if coroutines_dep.found()
  test_lua_sources += 'TestCoAwaitable.cxx'
endif

if lua_sodium_dep.found()
  test_lua_sources += 'TestLuaSodium.cxx'
endif

if lua_json_dep.found()
  test_lua_sources += 'TestLuaJson.cxx'
endif

test(
  'TestLua',
  executable(
    'TestLua',
    test_lua_sources,
    include_directories: inc,
    dependencies: [
      gtest,
      lua_dep,
      lua_event_dep,
      lua_json_dep,
      lua_sodium_dep,
    ],
  ),
)
