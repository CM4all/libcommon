libcrypto = dependency('libcrypto', version: '>= 1.1')
libssl = dependency('libssl', version: '>= 1.1')

crypto = static_library(
  'crypto',
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
    libcrypto,
  ],
)

crypto_dep = declare_dependency(
  link_with: crypto,
  dependencies: [
    util_dep,
    libcrypto,
  ],
)

ssl = static_library(
  'ssl',
  include_directories: inc,
  dependencies: [
    libssl,
  ],
)

ssl_dep = declare_dependency(
  link_with: ssl,
  dependencies: [
    crypto_dep,
    util_dep,
    libssl,
  ],
)
