if not uring_dep.found()
  subdir_done()
endif

executable(
  'UringCat',
  'UringCat.cxx',
  include_directories: inc,
  dependencies: [uring_dep],
)

executable(
  'UringNetCat',
  'UringNetCat.cxx',
  include_directories: inc,
  dependencies: [event_net_dep],
)
