system_sources = [
  'LargeAllocation.cxx',
  'BindMount.cxx',
  'ProcessName.cxx',
  'LinuxFD.cxx',
  'EpollFD.cxx',
  'Urandom.cxx',
]

system_dependencies = [
]

if get_variable('libcommon_enable_libcap', true)
  libcap = dependency('libcap')
  system_sources += 'CapabilityState.cxx'
  system_dependencies += libcap
endif

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
