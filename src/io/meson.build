io_sources = []

if compiler.get_id() != 'gcc' or compiler.version().version_compare('>=8')
  # the C++17 Filesystem TS is implemented in GCC>=8
  io_sources += [
    'FileLineParser.cxx',
    'ConfigParser.cxx',
  ]
endif

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
