libcap = dependency('libcap')

system = static_library(
  'system',
  'LargeAllocation.cxx',
  'BindMount.cxx',
  'CapabilityState.cxx',
  'ProcessName.cxx',
  'LinuxFD.cxx',
  'EpollFD.cxx',
  include_directories: inc,
  dependencies: [
    libcap,
  ],
)

system_dep = declare_dependency(
  link_with: system,
  dependencies: [
    io_dep,
    util_dep,
  ],
)
