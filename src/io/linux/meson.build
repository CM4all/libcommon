io_linux = static_library(
  'io',
  'ProcPid.cxx',
  'ProcFdinfo.cxx',
  'ProcCgroup.cxx',
  'MountInfo.cxx',
  include_directories: inc,
  dependencies: [
    fmt_dep,
  ],
)

io_linux_dep = declare_dependency(
  link_with: io_linux,
  dependencies: [
    io_dep,
  ],
)
