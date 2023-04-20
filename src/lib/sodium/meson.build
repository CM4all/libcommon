libsodium = dependency('libsodium', version: '>= 1.0.16',
                       required: get_variable('libcommon_require_sodium', true))
if not libsodium.found()
  sodium_dep = dependency('', required: false)
  subdir_done()
endif

sodium = static_library(
  'sodium',
  'Base64.cxx',
  include_directories: inc,
  dependencies: [
    libsodium,
  ],
)

sodium_dep = declare_dependency(
  link_with: sodium,
  dependencies: [
    libsodium,
  ],
)
