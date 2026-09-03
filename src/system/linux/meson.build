system_linux = static_library(
  'system_linux',
  'Mount.cxx',
  include_directories: inc,
  dependencies: [
    fmt_dep,
    io_dep,
  ],
)

system_linux_dep = declare_dependency(
  link_with: system_linux,
  dependencies: [
  ],
)
