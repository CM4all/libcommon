if not nettle_dep.found()
  subdir_done()
endif

test(
  'TestNettle',
  executable(
    'TestNettle',
    'TestSHA1.cxx',
    'TestSHA3.cxx',
    include_directories: inc,
    dependencies: [gtest, nettle_dep, util_dep],
  ),
)
