event_systemd_sources = []
event_systemd_dependencies = []

if is_variable('nlohmann_json_dep') and nlohmann_json_dep.found()
  event_systemd_sources += 'ResolvedClient.cxx'
  event_systemd_dependencies += nlohmann_json_dep
endif

event_systemd = static_library(
  'event_systemd',
  event_systemd_sources,
  'Watchdog.cxx',
  include_directories: inc,
  dependencies: [
    dependency('libsystemd'),
    event_systemd_dependencies,
    event_dep,
    net_dep,
  ],
)

event_systemd_dep = declare_dependency(
  link_with: event_systemd,
)
