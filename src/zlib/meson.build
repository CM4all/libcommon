zlib = dependency('zlib')

zlib = static_library(
  'zlib',
  'Error.cxx',
  'GunzipReader.cxx',
  'GzipOutputStream.cxx',
  include_directories: inc,
  dependencies: [
    zlib,
  ],
)

zlib_dep = declare_dependency(
  link_with: zlib,
  dependencies: [
    io_dep,
  ],
)
