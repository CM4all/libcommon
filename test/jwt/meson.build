if not jwt_dep.found()
  subdir_done()
endif

test(
  'TestJWT',
  executable(
    'TestJWT',
    'TestEdDSA.cxx',
    'TestES256.cxx',
    include_directories: inc,
    dependencies: [gtest, jwt_dep],
  ),
)
