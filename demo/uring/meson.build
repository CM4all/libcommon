if not uring_dep.found()
  subdir_done()
endif

executable(
  'UringCat',
  'UringCat.cxx',
  include_directories: inc,
  dependencies: [uring_dep],
)
