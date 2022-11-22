if compiler.get_id() == 'gcc'
  # libstdc++ has std::filesystem in a separate library and requires
  # this linker flag.
  std_filesystem_dep = declare_dependency(link_args: '-lstdc++fs')
else
  std_filesystem_dep = dependency('', required: false)
endif

io_config = static_library(
  'io_config',
  'LineParser.cxx',
  'FileLineParser.cxx',
  'ConfigParser.cxx',
  'IniParser.cxx',
  include_directories: inc,
  dependencies: [
    std_filesystem_dep,
  ],
)

io_config_dep = declare_dependency(
  link_with: io_config,
)
