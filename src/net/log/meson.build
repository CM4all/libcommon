net_log = static_library(
  'net',
  'ContentType.cxx',
  'String.cxx',
  'Parser.cxx',
  'OneLine.cxx',
  'Send.cxx',
  'Serializer.cxx',
  include_directories: inc,
  dependencies: [
    http_dep,
    net_dep,
  ],
)

net_log_dep = declare_dependency(
  link_with: net_log,
  dependencies: [
    http_dep,
  ],
)
