if not liburing.found()
  event_uring_dep = dependency('', required: false)
  subdir_done()
endif

event_uring = static_library(
  'event_uring',
  'Manager.cxx',
  'OpenStat.cxx',
  include_directories: inc,
  dependencies: [
    liburing,
  ],
)

event_uring_dep = declare_dependency(
  link_with: event_uring,
  dependencies: [
    uring_dep,
  ],
)
