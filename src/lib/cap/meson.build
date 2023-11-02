libcap = dependency('libcap',
                    required: get_variable('libcommon_require_cap', true))
if not libcap.found()
  cap_dep = dependency('', required: false)
  subdir_done()
endif

cap = static_library(
  'cap',
  [
    'Glue.cxx',
    'State.cxx',
  ],
  include_directories: inc,
  dependencies: [
    libcap,
  ]
)

cap_dep = declare_dependency(
  link_with: cap,
  dependencies: [
    libcap,
  ],
)
