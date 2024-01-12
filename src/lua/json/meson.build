lua_json = static_library(
  'lua_json',
  'Push.cxx',
  'ToJson.cxx',
  include_directories: inc,
  dependencies: [
    lua_dep,
    nlohmann_json_dep,
  ],
)

lua_json_dep = declare_dependency(
  link_with: lua_json,
  dependencies: [
    lua_dep,
    nlohmann_json_dep,
  ],
)
