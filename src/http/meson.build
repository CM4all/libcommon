http = static_library(
  'http',
  'Method.c',
  'Status.c',
  'HeaderName.cxx',
  'List.cxx',
  'Date.cxx',
  'Range.cxx',
  include_directories: inc,
  dependencies: [
    util_dep,
  ],
)

http_dep = declare_dependency(
  link_with: http,
  dependencies: [util_dep, time_dep],
)
