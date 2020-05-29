liburing = dependency('liburing', required: false)
if not liburing.found()
  uring_dep = dependency('', required: false)
  subdir_done()
endif

uring_sources = []

if get_option('coroutines')
  uring_sources += 'CoOperation.cxx'
endif

uring = static_library(
  'uring',
  'Ring.cxx',
  'Queue.cxx',
  'Operation.cxx',
  'OpenStat.cxx',
  uring_sources,
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
