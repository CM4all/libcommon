if not was_dep.found()
  was_json_dep = was_dep
  subdir_done()
endif

was_json = static_library(
  'was_json',
  'Read.cxx',
  'Write.cxx',
  include_directories: inc,
  dependencies: [
    was_dep,
    boost_json_dep,
  ],
)

was_json_dep = declare_dependency(
  link_with: was_json,
  dependencies: [
    boost_json_dep,
  ],
)
