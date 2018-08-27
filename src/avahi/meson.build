libavahi_client = dependency('avahi-client')

avahi = static_library(
  'avahi',
  'Check.cxx',
  'Client.cxx',
  'Poll.cxx',
  'Explorer.cxx',
  include_directories: inc,
  dependencies: [
    libavahi_client,
  ],
)

avahi_dep = declare_dependency(
  link_with: avahi,
  dependencies: [
    event_dep,
  ],
)
