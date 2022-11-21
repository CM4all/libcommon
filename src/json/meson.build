boost_json_inc = include_directories('../../boost/json/include', is_system: true)

boost_json_compile_args = [
  '-DBOOST_JSON_STANDALONE',
]

boost_json_dep = declare_dependency(
  compile_args: boost_json_compile_args,
  include_directories: [boost_json_inc, inc],
)

boost_json = static_library(
  'boost_json',
  'Boost.cxx',
  'Parse.cxx',
  'Serialize.cxx',
  'ToDisposableBuffer.cxx',
  'Was.cxx',
  dependencies: boost_json_dep,
  include_directories: boost_json_inc,
)

boost_json_dep = declare_dependency(
  link_with: boost_json,
  dependencies: boost_json_dep,
)
