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
)

event_dep = declare_dependency(
  link_with: event,
  dependencies: [
    system_dep,
    time_dep,
    util_dep,
  ],
)
