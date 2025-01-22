util_sources = [
  'AllocatedString.cxx',
  'DisposableBuffer.cxx',
  'Exception.cxx',
  'LeakDetector.cxx',
  'MimeType.cxx',
  'PrintException.cxx',
  'StringBuilder.cxx',
  'StringCompare.cxx',
  'StringParser.cxx',
  'StringStrip.cxx',
  'StringWithHash.cxx',
  'UTF8.cxx',
]

util = static_library(
  'util',
  util_sources,
  include_directories: inc,
)

util_dep = declare_dependency(
  link_with: util,
)
