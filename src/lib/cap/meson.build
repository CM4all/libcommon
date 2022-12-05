libcap = dependency('libcap')

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
