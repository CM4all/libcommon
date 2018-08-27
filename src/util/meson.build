util = static_library(
  'util',
  'AllocatedString.cxx',
  'Exception.cxx',
  'LeakDetector.cxx',
  'PrintException.cxx',
  'StringBuilder.cxx',
  'StringCompare.cxx',
  'StringParser.cxx',
  'StringStrip.cxx',
  'StringView.cxx',
  'HexFormat.c',
  'djbhash.c',
  include_directories: inc,
)

util_dep = declare_dependency(
  link_with: util,
)
