jwt = static_library(
  'jwt',
  'RS256.cxx',
  include_directories: inc,
  dependencies: [
    ssl_dep,
    sodium_dep,
  ],
)

jwt_dep = declare_dependency(
  link_with: jwt,
  dependencies: [
    ssl_dep,
  ],
)
