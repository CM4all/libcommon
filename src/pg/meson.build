libpq = dependency('libpq', version: '>= 9.2',
                   required: get_variable('libcommon_require_pg', true))
if not libpq.found()
  pg_dep = dependency('', required: false)
  subdir_done()
endif

if not is_variable('coroutines_dep')
  coroutines_dep = dependency('', required: false)
endif

pg_sources = []
pg_dependencies = []

if is_variable('time_dep')
  pg_sources += 'Timestamp.cxx'
  pg_dependencies += time_dep
endif

if is_variable('stock_dep')
  pg_sources += 'Stock.cxx'
  pg_dependencies += stock_dep
endif

if is_variable('event_dep')
  pg_sources += [
    'AsyncConnection.cxx',
    'SharedConnection.cxx',
  ]
  pg_dependencies += event_dep
endif

pg = static_library(
  'pg',
  'Serial.cxx',
  'Array.cxx',
  'Interval.cxx',
  'Connection.cxx',
  'Result.cxx',
  'Error.cxx',
  'Reflection.cxx',
  pg_sources,
  include_directories: inc,
  dependencies: [
    coroutines_dep,
    fmt_dep,
    libpq,
  ] + pg_dependencies,
)

pg_dep = declare_dependency(
  link_with: pg,
  dependencies: [
    coroutines_dep,
    util_dep,
    fmt_dep,
    libpq,
  ] + pg_dependencies,
)
