test(
  'TestSodium',
  executable(
    'TestSodium',
    'TestBase64.cxx',
    'TestGenericHash.cxx',
    'TestSHA256.cxx',
    'TestUrlSafeBase64SHA256.cxx',
    include_directories: inc,
    dependencies: [gtest, sodium_dep, util_dep],
  ),
)
