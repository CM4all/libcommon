libcrypto = dependency('libcrypto', version: '>= 1.1')

if get_variable('libcommon_enable_libssl', true)
  libssl = dependency('libssl', version: '>= 1.1')
else
  libssl = disabler()
endif

crypto_sources = []

if compiler.get_id() != 'gcc' or compiler.version().version_compare('>=10')
  # std::span is implemented in GCC>=10
  crypto_sources += [
    'SHA1.cxx',
    'SHA3.cxx',
  ]
endif

crypto = static_library(
  'crypto',
  crypto_sources,
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
