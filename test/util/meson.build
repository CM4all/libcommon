test_util_sources = []
if get_variable('libcommon_enable_boost', true)
  test_util_sources += 'TestVCircularBuffer.cxx'
endif

test(
  'TestUtil',
  executable(
    'TestUtil',
    test_util_sources,
    'TestCRC32.cxx',
    'TestException.cxx',
    'TestHashRing.cxx',
    'TestIntrusiveList.cxx',
    'TestIntrusiveForwardList.cxx',
    'TestFNVHash.cxx',
    'TestMimeType.cxx',
    'TestStringStrip.cxx',
    'TestTemplateString.cxx',
    include_directories: inc,
    dependencies: [gtest, util_dep],
  ),
)
