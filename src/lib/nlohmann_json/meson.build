nlohmann_json = dependency('nlohmann_json',
                           version: '>= 3.11',
                           fallback: ['nlohmann_json', 'nlohmann_json_multiple_headers'],
                           required: get_variable('libcommon_enable_json', true))
if not nlohmann_json.found()
  nlohmann_json_dep = nlohmann_json
  nlohmann_json_io_dep = nlohmann_json
  subdir_done()
endif

nlohmann_json_io = static_library(
  'nlohmann_json_io',
  'ToDisposableBuffer.cxx',
  dependencies: nlohmann_json,
  include_directories: inc,
)

# If you really need IO streams, you get this special dependency
nlohmann_json_io_dep = declare_dependency(
  link_with: nlohmann_json_io,
  dependencies: nlohmann_json,
)

nlohmann_json_dep = declare_dependency(
  # no iostreams, please
  compile_args: '-DJSON_NO_IO',
  dependencies: nlohmann_json,
)

nlohmann_json = static_library(
  'nlohmann_json',
  'ToDisposableBuffer.cxx',
  dependencies: nlohmann_json_dep,
  include_directories: inc,
)

nlohmann_json_dep = declare_dependency(
  link_with: nlohmann_json,
  dependencies: nlohmann_json_dep,
)
