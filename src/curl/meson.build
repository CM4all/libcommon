libcurl = dependency('libcurl', version: '>= 7.38')

curl_sources = [
  'Version.cxx',
  'Init.cxx',
]

curl_deps = []

if get_variable('libcommon_enable_event', true)
  curl_sources += [
    'Request.cxx',
    'Global.cxx',
  ]
  curl_deps += event_dep
endif

curl = static_library(
  'curl',
  curl_sources,
  include_directories: inc,
  dependencies: [
    libcurl,
  ],
)

curl_dep = declare_dependency(
  link_with: curl,
  dependencies: curl_deps,
)
