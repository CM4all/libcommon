executable(
  'Mirror',
  'Mirror.cxx',
  include_directories: inc,
  dependencies: [
    was_async_dep,
  ],
)

if get_option('coroutines')
  executable(
    'CoMirror',
    'CoMirror.cxx',
    include_directories: inc,
    dependencies: [
      was_async_dep,
    ],
  )

  executable(
    'Json',
    'Json.cxx',
    include_directories: inc,
    dependencies: [
      uri_dep,
      was_async_dep,
      boost_json_dep,
    ],
  )
endif
