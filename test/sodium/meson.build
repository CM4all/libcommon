if not sodium_dep.found()
  subdir_done()
endif

test(
  'TestSodium',
  executable(
    'TestSodium',
    'TestBase64.cxx',
    'TestGenericHash.cxx',
    'TestSHA256.cxx',
    'TestSHA512.cxx',
    'TestUrlSafeBase64SHA256.cxx',
    include_directories: inc,
    dependencies: [gtest, sodium_dep, util_dep],
  ),
)
