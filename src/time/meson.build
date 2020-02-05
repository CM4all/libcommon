time_sources = [
  'Convert.cxx',
  'ISO8601.cxx',
  'Parser.cxx',
  'Math.cxx',
  'gmtime.cxx',
]

time = static_library(
  'time',
  time_sources,
  include_directories: inc,
)

time_dep = declare_dependency(
  link_with: time,
)
