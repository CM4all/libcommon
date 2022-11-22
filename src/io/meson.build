io_sources = []
io_deps = []

io = static_library(
  'io',
  'FileDescriptor.cxx',
  'Open.cxx',
  'MakeDirectory.cxx',
  'RecursiveCopy.cxx',
  'RecursiveDelete.cxx',
  'DirectoryReader.cxx',
  'SmallFile.cxx',
  'WriteFile.cxx',
  'WriteBuffer.cxx',
  'MultiWriteBuffer.cxx',
  'FileWriter.cxx',
  'FdOutputStream.cxx',
  'FdReader.cxx',
  'BufferedOutputStream.cxx',
  'BufferedReader.cxx',
  'Logger.cxx',
  'ScopeChdir.cxx',
  'StringFile.cxx',
  'Temp.cxx',
  io_sources,
  include_directories: inc,
)

io_dep = declare_dependency(
  link_with: io,
  dependencies: [
    util_dep,
    io_deps,
  ],
)
