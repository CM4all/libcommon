was_json = static_library(
  'was_json',
  'Read.cxx',
  'Write.cxx',
  include_directories: inc,
  dependencies: [
    libwas,
    boost_json_dep,
  ],
)

was_json_dep = declare_dependency(
  link_with: was_json,
  dependencies: [
    boost_json_dep,
  ],
)
