if not sodium_dep.found() or not crypto_dep.found() or not nlohmann_json_dep.found()
  jwt_dep = dependency('', required: false)
  subdir_done()
endif

jwt = static_library(
  'jwt',
  'String.cxx',
  'ES256.cxx',
  'RS256.cxx',
  'EdDSA.cxx',
  'OsslJWK.cxx',
  'OsslJWS.cxx',
  include_directories: inc,
  dependencies: [
    crypto_dep,
    sodium_dep,
    nlohmann_json_dep,
  ],
)

jwt_dep = declare_dependency(
  link_with: jwt,
  dependencies: [
    crypto_dep,
    nlohmann_json_dep,
  ],
)
