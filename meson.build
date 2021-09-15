project('CM4all libcommon', ['c', 'cpp'], version: '0.1',
  meson_version: '>= 0.53',
  default_options: [
    'c_std=c11',
    'cpp_std=c++2a',
    'warning_level=3',
  ],
)

version_h = configuration_data()
version_h.set_quoted('PACKAGE', meson.project_name())
version_h.set_quoted('VERSION', meson.project_version())
configure_file(output: 'version.h', configuration: version_h)

debug = get_option('b_ndebug') != 'true'

compiler = meson.get_compiler('cpp')
c_compiler = meson.get_compiler('c')

common_flags = [
  '-D_REENTRANT', '-D_GNU_SOURCE',
]

test_common_flags = [
  '-Wcast-qual',
  '-Wcast-align',
  #'-Wdouble-promotion', (disabled due to boost::json)
  '-Wfloat-equal',
  '-Wmissing-declarations',
  '-Wmissing-format-attribute',
  '-Wmissing-noreturn',
  '-Wredundant-decls',
  '-Wshadow',
  '-Wsign-compare',
  '-Wundef',
  '-Wunused',
  '-Wvla',
  '-Wwrite-strings',

  '-Wno-long-long',

  # clang specific warning options:
  '-Wunreachable-code-aggressive',
  #'-Wused-but-marked-unused', (disabled due to OpenSSL)
]

test_global_common_flags = [
]

if debug
  common_flags += ['-DPOISON']
endif

test_global_cxxflags = test_global_common_flags + [
]

test_global_cflags = test_global_common_flags + [
]

test_cxxflags = test_common_flags + [
  '-fno-threadsafe-statics',
  '-fmerge-all-constants',

  '-Wcomma-subscript',
  '-Wextra-semi',
  '-Wmismatched-tags',
  '-Woverloaded-virtual',
  '-Wsign-promo',
  '-Wvolatile',
  '-Wvirtual-inheritance',

  '-Wno-missing-field-initializers',

  # a vtable without a dtor is just fine
  '-Wno-non-virtual-dtor',

  # the only warnings we got from this are from formatted error
  # messages, and their truncation is harmless
  '-Wno-format-truncation',

  # clang specific warning options:
  '-Wcomma',
  '-Wheader-hygiene',
  #'-Winconsistent-missing-destructor-override', (disabled due to boost::json)
]

if compiler.get_id() == 'gcc' and compiler.version().version_compare('>=11')
  # too many bogus warnings, e.g. for std::array::size()
  test_cxxflags += '-Wno-maybe-uninitialized'
endif

test_cflags = test_common_flags + [
  '-Waggregate-return',
  '-Wbad-function-cast',
  '-Wmissing-prototypes',
  '-Wnested-externs',
  '-Wstrict-prototypes',
]

add_global_arguments(common_flags, language: 'c')
add_global_arguments(common_flags, language: 'cpp')

add_global_arguments(compiler.get_supported_arguments(test_global_cxxflags), language: 'cpp')
add_global_arguments(c_compiler.get_supported_arguments(test_global_cflags), language: 'c')
add_project_arguments(compiler.get_supported_arguments(test_cxxflags), language: 'cpp')
add_project_arguments(c_compiler.get_supported_arguments(test_cflags), language: 'c')

if compiler.has_header('valgrind/memcheck.h')
  add_global_arguments('-DHAVE_VALGRIND_MEMCHECK_H', language: 'cpp')
  add_global_arguments('-DHAVE_VALGRIND_MEMCHECK_H', language: 'c')
endif

if get_option('coroutines')
  if compiler.get_id() == 'clang'
    add_global_arguments('-fcoroutines-ts', '-stdlib=libc++', language: 'cpp')
    add_global_link_arguments('-stdlib=libc++', language: 'cpp')
  else
    add_global_arguments('-fcoroutines', language: 'cpp')
  endif
endif

if compiler.get_id() == 'clang'
  gtest_sub = subproject('gtest')
endif

inc = include_directories('src', 'fake', '.')

subdir('src/util')
subdir('src/uri')
subdir('src/time')
subdir('src/http')

if compiler.get_id() != 'gcc' or compiler.version().version_compare('>=10')
  # this library checks __cpp_constinit and requires
  # std::memory_resource, so disable it with older GCC
  subdir('src/json')
endif

subdir('src/adata')
subdir('src/lua')
subdir('src/io')
subdir('src/io/uring')
subdir('src/system')
subdir('src/ssl')

if compiler.get_id() != 'gcc' or compiler.version().version_compare('>=8')
  # this library needs std::byte
  subdir('src/sodium')
  subdir('src/jwt')
endif

subdir('src/net')
subdir('src/event')
subdir('src/event/net')
subdir('src/event/net/cares')
subdir('src/event/uring')

subdir('src/avahi')
if avahi_dep.found()
  subdir('demo/avahi')
endif

subdir('src/curl')
subdir('src/zlib')
subdir('src/pcre')
subdir('src/pg')
subdir('src/odbus')
subdir('src/spawn')
subdir('src/stock')
subdir('src/translation')
subdir('src/was')
subdir('src/was/async')

subdir('src/lua/event')
subdir('src/lua/pg')

if compiler.get_id() != 'gcc' or compiler.version().version_compare('>=8')
  subdir('src/translation/server')
endif

subdir('test')
