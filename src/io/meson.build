io_sources = []
io_deps = []

if get_variable('libcommon_enable_filesystem', true) and (compiler.get_id() != 'gcc' or compiler.version().version_compare('>=8'))
  # the C++17 Filesystem TS is implemented in GCC>=8
  io_sources += [
    'FileLineParser.cxx',
    'ConfigParser.cxx',
  ]

  if compiler.get_id() == 'gcc'
    # libstdc++ has std::filesystem in a separate library and requires
    # this linker flag.
    io_deps += declare_dependency(link_args: '-lstdc++fs')
  endif
endif

io = static_library(
  'io',
  'FileDescriptor.cxx',
  'Open.cxx',
  'MakeDirectory.cxx',
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
  'LineParser.cxx',
  'Logger.cxx',
  'StringFile.cxx',
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
