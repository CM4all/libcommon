executable(
  'RunHttpGet',
  'RunHttpGet.cxx',
  include_directories: inc,
  dependencies: [
    curl_dep,
  ],
)

if get_option('coroutines')
  executable(
    'RunCoHttpGet',
    'RunCoHttpGet.cxx',
    include_directories: inc,
    dependencies: [
      curl_dep,
    ],
  )
endif
