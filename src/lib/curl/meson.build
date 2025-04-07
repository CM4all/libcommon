libcurl = dependency('libcurl', version: '>= 7.38',
                     required: get_variable('libcommon_require_curl', true))
if not libcurl.found()
  curl_dep = libcurl
  subdir_done()
endif

curl_sources = [
  'Version.cxx',
  'Init.cxx',
  'StringHandler.cxx',
  'StringGlue.cxx',
]

if not is_variable('coroutines_dep')
  coroutines_dep = dependency('', required: false)
endif

curl_deps = [
  coroutines_dep,
]

if is_variable('event_dep')
  curl_sources += [
    'Adapter.cxx',
    'Request.cxx',
    'Setup.cxx',
    'Global.cxx',
  ]
  curl_deps += event_dep

  if coroutines_dep.found()
    curl_sources += 'CoRequest.cxx'
  endif
endif

curl = static_library(
  'curl',
  curl_sources,
  include_directories: inc,
  dependencies: [
    libcurl,
    coroutines_dep,
  ],
)

curl_dep = declare_dependency(
  link_with: curl,
  dependencies: curl_deps,
)
