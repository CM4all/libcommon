time_sources = [
  'Convert.cxx',
  'ISO8601.cxx',
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
