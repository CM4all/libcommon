if not pg_dep.found()
  subdir_done()
endif

test(
  'TestPg',
  executable(
    'TestPg',
    'TestDecodeArray.cxx',
    'TestEncodeArray.cxx',
    'TestParamWrapper.cxx',
    'TestInterval.cxx',
    'TestTimestamp.cxx',
    include_directories: inc,
    dependencies: [gtest, pg_dep, time_dep, util_dep],
  ),
)
