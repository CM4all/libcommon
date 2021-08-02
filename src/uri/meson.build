uri = static_library(
  'uri',
  'Verify.cxx',
  'Extract.cxx',
  'EmailAddress.cxx',
  'Escape.cxx',
  'Unescape.cxx',
  'MapQueryString.cxx',
  include_directories: inc,
  dependencies: util_dep,
)

uri_dep = declare_dependency(
  link_with: uri,
)
