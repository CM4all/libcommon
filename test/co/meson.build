if pg_dep.found()
  executable(
    'RunCoQuery',
    'RunCoQuery.cxx',
    include_directories: inc,
    dependencies: [
      pg_dep, stock_dep,
      coroutines_dep,
    ],
  )
endif

if event_net_cares_dep.found()
  executable(
    'RunCoLookup',
    'RunCoLookup.cxx',
    include_directories: inc,
    dependencies: [
      event_net_cares_dep,
    ],
  )
endif

if event_uring_dep.found()
  executable(
    'RunCoReadTextFile',
    'RunCoReadTextFile.cxx',
    include_directories: inc,
    dependencies: [
      event_uring_dep, stock_dep,
    ],
  )
endif

test(
  'TestCo',
  executable(
    'TestCo',
    'TestInvokeTask.cxx',
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
