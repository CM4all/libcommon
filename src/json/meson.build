boost_json_inc = include_directories('../../boost/json/include')

boost_json_compile_args = [
  '-DBOOST_JSON_STANDALONE',
]

if compiler.get_id() == 'clang'
  boost_json_compile_args += [
    # clang warns about #warning which is used by boost::json
    '-Wno-#warnings',

    # libc++11 doesn't have std::memory_resource; disable the #warning
    # "Support for std::memory_resource is required to use Boost.JSON
    # standalone, using std::experimental::memory_resource as
    # fallback"
    '-Wno-pedantic',

    # suppress clang 13 warning on the defaulted
    # boost::json::const_stream copy constructor
    '-Wno-deprecated-copy',
  ]
endif

boost_json_dep = declare_dependency(
  compile_args: boost_json_compile_args,
  include_directories: [boost_json_inc, inc],
)

boost_json = static_library(
  'boost_json',
  'Boost.cxx',
  'Serialize.cxx',
  'ToDisposableBuffer.cxx',
  dependencies: boost_json_dep,
  include_directories: boost_json_inc,
)

boost_json_dep = declare_dependency(
  link_with: boost_json,
  dependencies: boost_json_dep,
)
