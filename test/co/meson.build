test(
  'TestCo',
  executable(
    'TestCo',
    'TestInvokeTask.cxx',
    'TestEagerInvokeTask.cxx',
    'TestEagerTask.cxx',
    'TestAll.cxx',
    'TestCoCache.cxx',
    'TestMultiAwaitable.cxx',
    include_directories: inc,
    dependencies: [
      gtest,
      event_dep,
      coroutines_dep,
    ],
  ),
)
