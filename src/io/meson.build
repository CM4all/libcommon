io_sources = []

if get_variable('libcommon_enable_boost', true)
  boost = dependency('boost', modules: ['system', 'filesystem'], version: '>= 1.54')
  io_sources += [
    'FileLineParser.cxx',
    'ConfigParser.cxx',
  ]
else
  boost = dependency('', required: false)
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
  dependencies: [
    boost,
  ],
)

io_dep = declare_dependency(
  link_with: io,
  dependencies: [
    util_dep,
  ],
)
