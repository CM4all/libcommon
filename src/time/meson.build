time_sources = [
  'Calendar.cxx',
  'Convert.cxx',
  'ISO8601.cxx',
  'Math.cxx',
]

if get_variable('libcommon_enable_c', true)
  time_sources += [
    'gmtime.c',
  ]
endif

time = static_library(
  'time',
  time_sources,
  include_directories: inc,
)

time_dep = declare_dependency(
  link_with: time,
)
