io_linux = static_library(
  'io',
  'MountInfo.cxx',
  include_directories: inc,
)

io_linux_dep = declare_dependency(
  link_with: io_linux,
  dependencies: [
    io_dep,
  ],
)
