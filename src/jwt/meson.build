jwt = static_library(
  'jwt',
  'RS256.cxx',
  'EdDSA.cxx',
  include_directories: inc,
  dependencies: [
    crypto_dep,
    sodium_dep,
  ],
)

jwt_dep = declare_dependency(
  link_with: jwt,
  dependencies: [
    crypto_dep,
  ],
)
