libdbus = dependency('dbus-1')

odbus = static_library(
  'odbus',
  'Error.cxx',
  'Connection.cxx',
  'Message.cxx',
  'Watch.cxx',
  'ScopeMatch.cxx',
  include_directories: inc,
  dependencies: [
    libdbus,
  ],
)

odbus_dep = declare_dependency(
  link_with: odbus,
)
