if not lua_dep.found()
  subdir_done()
endif

test_lua_sources = [
  'TestClass.cxx',
  'TestCoRunner.cxx',
  'TestForEach.cxx',
]

if coroutines_dep.found()
  test_lua_sources += 'TestCoAwaitable.cxx'
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
    ],
  ),
)
