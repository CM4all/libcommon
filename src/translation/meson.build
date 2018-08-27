translation = static_library(
  'translation',
  'PReader.cxx',
  'Parser.cxx',
  'Response.cxx',
  include_directories: inc,
)

translation_dep = declare_dependency(
  link_with: translation,
  dependencies: [
    spawn_dep,
    net_dep,
  ],
)
