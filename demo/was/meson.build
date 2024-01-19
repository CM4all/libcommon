executable(
  'Mirror',
  'Mirror.cxx',
  include_directories: inc,
  dependencies: [
    was_server_async_dep,
  ],
)

if coroutines_dep.found()
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

  if nlohmann_json_dep.found()
    executable(
      'Json',
      'Json.cxx',
      include_directories: inc,
      dependencies: [
        uri_dep,
        was_server_async_dep,
        was_async_nlohmann_json_dep,
      ],
    )
  endif

  executable(
    'BadRequest',
    'BadRequest.cxx',
    include_directories: inc,
    dependencies: [
      uri_dep,
      was_server_async_dep,
    ],
  )
endif
