net_log_types = static_library(
  'net_log_types',
  'ContentType.cxx',
  'String.cxx',
  include_directories: inc,
  dependencies: [
    util_dep,
  ],
)

net_log_types_dep = declare_dependency(
  link_with: net_log_types,
)

net_log = static_library(
  'net_log',
  'ContentType.cxx',
  'String.cxx',
  'Parser.cxx',
  'OneLine.cxx',
  'Send.cxx',
  'Serializer.cxx',
  include_directories: inc,
  dependencies: [
    net_log_types_dep,
    http_dep,
    net_dep,
  ],
)

net_log_dep = declare_dependency(
  link_with: net_log,
)
