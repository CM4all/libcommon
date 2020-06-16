if compiler.get_id() != 'gcc' or compiler.version().version_compare('>=8')
  test(
    'TestIo',
    executable(
      'TestIo',
      'TestConfigParser.cxx',
      include_directories: inc,
      dependencies: [gtest, io_dep, util_dep],
    ),
  )
endif
