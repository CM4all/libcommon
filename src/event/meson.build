boost = dependency('boost', version: '>= 1.54')

event = static_library(
  'event',
  'Loop.cxx',
  'ShutdownListener.cxx',
  'TimerEvent.cxx',
  'CleanupTimer.cxx',
  'DeferEvent.cxx',
  'SocketEvent.cxx',
  'SignalEvent.cxx',
  'PipeLineReader.cxx',
  include_directories: inc,
  dependencies: [
    boost,
  ],
)

event_dep = declare_dependency(
  link_with: event,
  dependencies: [
    system_dep,
    util_dep,
    boost,
  ],
)
