executable(
  'RunCoQuery',
  'RunCoQuery.cxx',
  include_directories: inc,
  dependencies: [
    pg_dep, stock_dep,
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
    'TestCoCache.cxx',
    include_directories: inc,
    dependencies: [gtest, event_dep],
  ),
)
