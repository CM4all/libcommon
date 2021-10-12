libpq = dependency('libpq', version: '>= 9.2',
                   required: get_variable('libcommon_require_pg', true))
if not libpq.found()
  pg_dep = dependency('', required: false)
  subdir_done()
endif

pg = static_library(
  'pg',
  'Serial.cxx',
  'Array.cxx',
  'Interval.cxx',
  'Timestamp.cxx',
  'Connection.cxx',
  'AsyncConnection.cxx',
  'Result.cxx',
  'Error.cxx',
  'Reflection.cxx',
  'Stock.cxx',
  include_directories: inc,
  dependencies: [
    libpq,
  ],
)

pg_dep = declare_dependency(
  link_with: pg,
  dependencies: [
    event_dep,
    util_dep,
    libpq,
  ],
)
