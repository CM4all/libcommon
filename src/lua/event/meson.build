lua_event = static_library(
  'lua_event',
  'Init.cxx',
  'Timer.cxx',
  include_directories: inc,
  dependencies: [
    lua_dep,
    event_dep,
  ],
)

lua_event_dep = declare_dependency(
  link_with: lua_event,
)
