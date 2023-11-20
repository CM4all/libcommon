if not was_dep.found()
  was_nlohmann_json_dep = was_dep
  subdir_done()
endif

was_nlohmann_json = static_library(
  'was_json',
  'Read.cxx',
  'Write.cxx',
  include_directories: inc,
  dependencies: [
    was_dep,
    nlohmann_json_dep,
  ],
)

was_nlohmann_json_dep = declare_dependency(
  link_with: was_nlohmann_json,
  dependencies: [
    nlohmann_json_dep,
  ],
)
