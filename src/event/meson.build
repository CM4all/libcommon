if get_variable('libcommon_enable_boost', true)
  event_boost_dep = dependency('boost', version: '>= 1.54')
else
  event_boost_dep = dependency('', required: false)
endif

event_sources = []

if get_variable('libcommon_enable_FineTimerEvent', true)
  event_sources += [
    'FineTimerEvent.cxx',
    'TimerList.cxx',
  ]
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
    event_boost_dep,
  ],
)

event_dep = declare_dependency(
  link_with: event,
  dependencies: [
    system_dep,
    util_dep,
    event_boost_dep,
  ],
)
