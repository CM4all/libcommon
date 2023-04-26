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

system_dependencies = [
]

system = static_library(
  'system',
  system_sources,
  include_directories: inc,
  dependencies: system_dependencies,
)

system_dep = declare_dependency(
  link_with: system,
  dependencies: [
    io_dep,
    util_dep,
  ],
)
