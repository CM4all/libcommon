threads_dep = dependency('threads')

thread_pool = static_library(
  'thread_queue',
  'Queue.cxx',
  'Worker.cxx',
  'Pool.cxx',
  'Notify.cxx',
  include_directories: inc,
  dependencies: [
    event_dep,
    util_dep,
    threads_dep,
  ],
)

thread_pool_dep = declare_dependency(
  link_with: thread_pool,
  dependencies: [
    event_dep,
    util_dep,
    threads_dep,
  ],
)
