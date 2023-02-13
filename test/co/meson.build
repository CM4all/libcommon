executable(
  'RunCoQuery',
  'RunCoQuery.cxx',
  include_directories: inc,
  dependencies: [
    pg_dep, stock_dep,
    coroutines_dep,
  ],
)

executable(
  'RunCoLookup',
  'RunCoLookup.cxx',
  include_directories: inc,
  dependencies: [
    event_net_cares_dep,
  ],
)

executable(
  'RunCoReadTextFile',
  'RunCoReadTextFile.cxx',
  include_directories: inc,
  dependencies: [
    event_uring_dep, stock_dep,
  ],
)

test(
  'TestCo',
  executable(
    'TestCo',
    'TestEagerTask.cxx',
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
