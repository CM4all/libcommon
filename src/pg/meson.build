libpq = dependency('libpq', version: '>= 8.4')

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
  include_directories: inc,
  dependencies: [
    libpq,
  ],
)

pg_dep = declare_dependency(
  link_with: pg,
  dependencies: [
    event_dep,
  ],
)
