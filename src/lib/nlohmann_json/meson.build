nlohmann_json = dependency('nlohmann_json',
                           version: '>= 3.11',
                           required: get_variable('libcommon_enable_json', true))
if not nlohmann_json.found()
  nlohmann_json_dep = nlohmann_json
  subdir_done()
endif

nlohmann_json_dep = declare_dependency(
  # no iostreams, please
  compile_args: '-DJSON_NO_IO',
  dependencies: nlohmann_json,
)
