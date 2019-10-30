http_sources = [
  'HeaderName.cxx',
  'List.cxx',
  'Date.cxx',
  'Range.cxx',
]

if get_variable('libcommon_enable_c', true)
  http_sources += [
    'Method.c',
    'Status.c',
  ]
endif

http = static_library(
  'http',
  http_sources,
  include_directories: inc,
  dependencies: [
    util_dep,
  ],
)

http_dep = declare_dependency(
  link_with: http,
  dependencies: [util_dep, time_dep],
)
