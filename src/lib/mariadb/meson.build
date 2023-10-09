libmariadb = dependency('libmariadb',
                        required: get_variable('libcommon_require_mariadb', true))
if not libmariadb.found()
  mariadb_dep = dependency('', required: false)
  subdir_done()
endif

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
