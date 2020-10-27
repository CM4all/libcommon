libavahi_client = dependency('avahi-client', required: get_variable('libcommon_require_avahi', true))
if not libavahi_client.found()
  avahi_dep = dependency('', required: false)
  subdir_done()
endif

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
    net_dep,
  ],
)
