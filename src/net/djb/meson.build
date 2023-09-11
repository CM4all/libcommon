net_djb = static_library(
  'net_djb',
  'NetstringInput.cxx',
  'NetstringHeader.cxx',
  'NetstringGenerator.cxx',
  include_directories: inc,
  dependencies: [
    fmt_dep,
  ],
)

net_djb_dep = declare_dependency(
  link_with: net_djb,
)
