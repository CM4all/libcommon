system_sources = [
  'PageAllocator.cxx',
  'LargeAllocation.cxx',
  'Mount.cxx',
  'ProcessName.cxx',
  'KernelVersion.cxx',
  'LinuxFD.cxx',
  'EpollFD.cxx',
  'Urandom.cxx',
]

system = static_library(
  'system',
  system_sources,
  include_directories: inc,
  dependencies: [
    fmt_dep,
  ],
)

system_dep = declare_dependency(
  link_with: system,
  dependencies: [
    io_dep,
    util_dep,
  ],
)
