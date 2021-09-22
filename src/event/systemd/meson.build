event_systemd = static_library(
  'event_systemd',
  'Watchdog.cxx',
  include_directories: inc,
  dependencies: [
    dependency('libsystemd'),
    event_dep,
  ],
)

event_systemd_dep = declare_dependency(
  link_with: event_systemd,
)
