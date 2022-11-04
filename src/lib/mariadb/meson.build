libmariadb = dependency('libmariadb')

mariadb = static_library(
  'mariadb',
  'Connection.cxx',
  'Statement.cxx',
  include_directories: inc,
  dependencies: [
    libmariadb,
  ],
)

mariadb_dep = declare_dependency(
  link_with: mariadb,
  dependencies: [
    libmariadb,
  ],
)
