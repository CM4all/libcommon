liburing = dependency('liburing',
                      required: get_variable('libcommon_require_uring', false))

uring_config = configuration_data()
uring_config.set('HAVE_URING', liburing.found())
configure_file(output: 'config.h', configuration: uring_config)

if not liburing.found()
  uring_dep = dependency('', required: false)
  subdir_done()
endif

uring_sources = []

if not is_variable('coroutines_dep')
  coroutines_dep = dependency('', required: false)
endif

if coroutines_dep.found()
  uring_sources += ['CoOperation.cxx', 'CoTextFile.cxx']
endif

uring = static_library(
  'uring',
  'Ring.cxx',
  'Queue.cxx',
  'Operation.cxx',
  'Open.cxx',
  'OpenStat.cxx',
  'Close.cxx',
  uring_sources,
  include_directories: inc,
  dependencies: [
    liburing,
    coroutines_dep,
  ],
)

uring_dep = declare_dependency(
  link_with: uring,
  dependencies: [
    liburing,
    io_dep,
    coroutines_dep,
  ],
)
