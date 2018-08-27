libcurl = dependency('libcurl', version: '>= 7.38')

curl = static_library(
  'curl',
  'Version.cxx',
  'Request.cxx',
  'Global.cxx',
  'Init.cxx',
  include_directories: inc,
  dependencies: [
    libcurl,
  ],
)

curl_dep = declare_dependency(
  link_with: curl,
  dependencies: [
    event_dep,
  ],
)
