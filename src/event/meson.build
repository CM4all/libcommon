event_config = configuration_data()
event_have_uring = is_variable('liburing') and uring_dep.found()
event_config.set('HAVE_URING', event_have_uring)
configure_file(output: 'config.h', configuration: event_config)

event_sources = []
event_internal_dependencies = []

if get_variable('libcommon_enable_FineTimerEvent', true)
  event_sources += [
    'FineTimerEvent.cxx',
    'TimerList.cxx',
  ]
endif

if event_have_uring
  event_sources += 'uring/Manager.cxx'
  event_internal_dependencies += uring_dep
endif

event = static_library(
  'event',
  event_sources,
  'Loop.cxx',
  'ShutdownListener.cxx',
  'TimerWheel.cxx',
  'CoarseTimerEvent.cxx',
  'CleanupTimer.cxx',
  'DeferEvent.cxx',
  'InotifyEvent.cxx',
  'SocketEvent.cxx',
  'SignalEvent.cxx',
  'PipeLineReader.cxx',
  include_directories: inc,
  dependencies: [
    fmt_dep,
    event_internal_dependencies,
  ],
)

event_dep = declare_dependency(
  link_with: event,
  dependencies: [
    system_dep,
    util_dep,
  ],
)
