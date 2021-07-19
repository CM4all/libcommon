libssl = dependency('openssl', version: '>= 1.1')

ssl = static_library(
  'ssl',
  'AltName.cxx',
  'Buffer.cxx',
  'Request.cxx',
  'Certificate.cxx',
  'Dummy.cxx',
  'Edit.cxx',
  'Error.cxx',
  'Hash.cxx',
  'Key.cxx',
  'LoadFile.cxx',
  'Name.cxx',
  'Time.cxx',
  include_directories: inc,
  dependencies: [
    libssl,
  ],
)

ssl_dep = declare_dependency(
  link_with: ssl,
  dependencies: [
    util_dep,
  ],
)
