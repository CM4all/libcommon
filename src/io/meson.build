boost = dependency('boost', modules: ['system', 'filesystem'], version: '>= 1.54')

io = static_library(
  'io',
  'FileDescriptor.cxx',
  'WriteFile.cxx',
  'WriteBuffer.cxx',
  'MultiWriteBuffer.cxx',
  'FileWriter.cxx',
  'LineParser.cxx',
  'FileLineParser.cxx',
  'ConfigParser.cxx',
  'Logger.cxx',
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
