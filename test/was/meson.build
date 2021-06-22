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
endif
