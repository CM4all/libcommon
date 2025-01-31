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

if uring_dep.found()
  executable(
    'RunCoReadTextFile',
    'RunCoReadTextFile.cxx',
    include_directories: inc,
    dependencies: [
      event_dep, stock_dep,
    ],
  )
endif

if nlohmann_json.found()
  executable(
    'RunCoResolvedClient',
    'RunCoResolvedClient.cxx',
    include_directories: inc,
    dependencies: [
      event_systemd_dep,
    ],
  )
endif
