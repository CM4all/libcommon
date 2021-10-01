if compiler.get_id() == 'gcc' and compiler.version().version_compare('<10')
  subdir_done()
endif

test(
  'TestSsl',
  executable(
    'TestSsl',
    'TestSHA1.cxx',
    'TestSHA3.cxx',
    include_directories: inc,
    dependencies: [gtest, ssl_dep],
  ),
)
