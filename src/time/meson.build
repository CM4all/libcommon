time = static_library(
  'time',
  'gmtime.c',
  'Convert.cxx',
  'ISO8601.cxx',
  include_directories: inc,
)

time_dep = declare_dependency(
  link_with: time,
)
