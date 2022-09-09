libcares = dependency('libcares')

event_net_cares_sources = []

if not is_variable('coroutines_dep')
  coroutines_dep = dependency('', required: false)
endif

if coroutines_dep.found()
  event_net_cares_sources += 'CoLookup.cxx'
endif

event_net_cares = static_library(
  'event_net_cares',
  'Error.cxx',
  'Init.cxx',
  'Channel.cxx',
  event_net_cares_sources,
  include_directories: inc,
  dependencies: [
    libcares,
    coroutines_dep,
  ],
)

event_net_cares_dep = declare_dependency(
  link_with: event_net_cares,
  dependencies: [
    event_dep,
    net_dep,
    util_dep,
    coroutines_dep,
  ],
)
