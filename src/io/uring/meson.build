liburing = dependency('liburing', required: false)
if not liburing.found()
  uring_dep = dependency('', required: false)
  subdir_done()
endif

uring = static_library(
  'uring',
  'Ring.cxx',
  'Queue.cxx',
  'Operation.cxx',
  include_directories: inc,
  dependencies: [
    liburing,
  ],
)

uring_dep = declare_dependency(
  compile_args: ['-DHAVE_URING'],
  link_with: uring,
  dependencies: [
    liburing,
    io_dep,
  ],
)
