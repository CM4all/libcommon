net_djb = static_library(
  'net_djb',
  'NetstringInput.cxx',
  'NetstringHeader.cxx',
  'NetstringGenerator.cxx',
  include_directories: inc,
  dependencies: [
    fmt_dep,
    system_dep,
    net_dep,
    io_dep,
    util_dep,
  ],
)

net_djb_dep = declare_dependency(
  link_with: net_djb,
)
