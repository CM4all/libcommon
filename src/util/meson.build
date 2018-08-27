util_sources = [
  'AllocatedString.cxx',
  'Exception.cxx',
  'LeakDetector.cxx',
  'PrintException.cxx',
  'StringBuilder.cxx',
  'StringCompare.cxx',
  'StringParser.cxx',
  'StringStrip.cxx',
  'StringView.cxx',
]

if get_variable('libcommon_enable_c', true)
  util_sources += [
    'HexFormat.c',
    'djbhash.c',
  ]
endif

util = static_library(
  'util',
  util_sources,
  include_directories: inc,
)

util_dep = declare_dependency(
  link_with: util,
)
