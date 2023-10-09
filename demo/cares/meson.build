if not event_net_cares_dep.found()
  subdir_done()
endif

executable(
  'RunCares',
  'RunCares.cxx',
  include_directories: inc,
  dependencies: [
    event_net_cares_dep,
  ],
)
