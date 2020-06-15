io_sources = []

io = static_library(
  'io',
  'FileDescriptor.cxx',
  'Open.cxx',
  'MakeDirectory.cxx',
  'DirectoryReader.cxx',
  'WriteFile.cxx',
  'WriteBuffer.cxx',
  'MultiWriteBuffer.cxx',
  'FileWriter.cxx',
  'FdOutputStream.cxx',
  'FdReader.cxx',
  'BufferedOutputStream.cxx',
  'BufferedReader.cxx',
  'LineParser.cxx',
  'FileLineParser.cxx',
  'ConfigParser.cxx',
  'Logger.cxx',
  io_sources,
  include_directories: inc,
)

io_dep = declare_dependency(
  link_with: io,
  dependencies: [
    util_dep,
  ],
)
