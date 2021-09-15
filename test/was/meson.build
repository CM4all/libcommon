executable(
  'Mirror',
  'Mirror.cxx',
  include_directories: inc,
  dependencies: [
    was_server_async_dep,
  ],
)

if get_option('coroutines')
  executable(
    'CoMirror',
    'CoMirror.cxx',
    include_directories: inc,
    dependencies: [
      was_server_async_dep,
    ],
  )

  executable(
    'CoSleepMirror',
    'CoSleepMirror.cxx',
    include_directories: inc,
    dependencies: [
      was_server_async_dep,
    ],
  )

  executable(
    'Json',
    'Json.cxx',
    include_directories: inc,
    dependencies: [
      uri_dep,
      was_server_async_dep,
      boost_json_dep,
    ],
  )

  executable(
    'BadRequest',
    'BadRequest.cxx',
    include_directories: inc,
    dependencies: [
      uri_dep,
      was_server_async_dep,
      boost_json_dep,
    ],
  )
endif
