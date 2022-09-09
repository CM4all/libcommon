executable(
  'RunHttpGet',
  'RunHttpGet.cxx',
  include_directories: inc,
  dependencies: [
    curl_dep,
  ],
)

if coroutines_dep.found()
  executable(
    'RunCoHttpGet',
    'RunCoHttpGet.cxx',
    include_directories: inc,
    dependencies: [
      curl_dep,
    ],
  )
endif
