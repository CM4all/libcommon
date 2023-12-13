memory = static_library(
  'memory',
  'SlicePool.cxx',
  'SliceAllocation.cxx',
  'SliceFifoBuffer.cxx',
  include_directories: inc,
  dependencies: [
    util_dep,
    system_dep,
  ]
)

memory_dep = declare_dependency(
  link_with: memory,
)
