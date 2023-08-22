if not pcre_dep.found()
  subdir_done()
endif

test(
  'TestPcre',
  executable(
    'TestPcre',
    'TestMatch.cxx',
    include_directories: inc,
    dependencies: [
      gtest,
      pcre_dep,
    ],
  ),
)
