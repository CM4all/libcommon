libsodium = dependency('libsodium', version: '>= 1.0.16')

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
