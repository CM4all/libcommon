if not is_variable('openssl_min_version')
  openssl_min_version = '1.1.1'
endif

if not is_variable('openssl_api_compat')
  openssl_api_compat = '0x10100000L'
endif

libcrypto = dependency('libcrypto', version: '>= ' + openssl_min_version,
                       required: get_variable('libcommon_require_libcrypto', true))
if not libcrypto.found()
  crypto_dep = libcrypto
  subdir_done()
endif

libcrypto = declare_dependency(
  compile_args: ['-DOPENSSL_API_COMPAT=' + openssl_api_compat],
  dependencies: libcrypto,
)

if get_variable('libcommon_enable_libssl', true)
  libssl = dependency('libssl', version: '>= ' + openssl_min_version)

  libssl = declare_dependency(
    compile_args: ['-DOPENSSL_API_COMPAT=' + openssl_api_compat],
    dependencies: libssl,
  )
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
  'EvpParam.cxx',
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
  'TrivialExDataIndex.cxx',
  include_directories: inc,
  dependencies: [
    libcrypto,
    fmt_dep,
  ],
)

crypto_dep = declare_dependency(
  link_with: crypto,
  dependencies: [
    util_dep,
    libcrypto,
  ],
)

ssl_dep = declare_dependency(
  dependencies: [
    crypto_dep,
    util_dep,
    libssl,
  ],
)
