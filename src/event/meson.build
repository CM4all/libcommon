if get_variable('libcommon_enable_boost', true)
  event_boost_dep = dependency('boost', version: '>= 1.54')
else
  event_boost_dep = dependency('', required: false)
endif

event = static_library(
  'event',
  'Loop.cxx',
  'ShutdownListener.cxx',
  'TimerWheel.cxx',
  'TimerList.cxx',
  'CoarseTimerEvent.cxx',
  'FineTimerEvent.cxx',
  'CleanupTimer.cxx',
  'DeferEvent.cxx',
  'SocketEvent.cxx',
  'SignalEvent.cxx',
  'PipeLineReader.cxx',
  include_directories: inc,
  dependencies: [
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
