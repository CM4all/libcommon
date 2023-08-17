libmariadb = dependency('libmariadb')

mariadb = static_library(
  'mariadb',
  'Error.cxx',
  'Connection.cxx',
  'Statement.cxx',
  include_directories: inc,
  dependencies: [
    libmariadb,
    fmt_dep,
  ],
)

mariadb_dep = declare_dependency(
  link_with: mariadb,
  dependencies: [
    libmariadb,
  ],
)
