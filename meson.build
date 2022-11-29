project('CM4all libcommon', ['c', 'cpp'], version: '0.1',
  meson_version: '>= 0.56',
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
  '-Wdouble-promotion',
  '-Wfloat-equal',
  '-Wmissing-declarations',
  '-Wmissing-format-attribute',
  '-Wmissing-noreturn',
  '-Wredundant-decls',
  '-Wshadow',
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
  '-Winconsistent-missing-destructor-override',
]

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

inc = include_directories('src', 'fake', '.')

subdir('src/util')
subdir('src/uri')
subdir('src/time')
subdir('src/http')

if get_option('coroutines')
  subdir('src/co')
endif

if compiler.get_id() != 'gcc' or compiler.version().version_compare('>=10')
  # this library checks __cpp_constinit and requires
  # std::memory_resource, so disable it with older GCC
  subdir('src/json')
  libcommon_enable_json = true
endif

subdir('src/adata')
subdir('src/lua')
subdir('src/io')
subdir('src/io/config')
subdir('src/io/uring')
subdir('src/system')
subdir('src/lib/openssl')

if compiler.get_id() != 'gcc' or compiler.version().version_compare('>=8')
  # this library needs std::byte
  subdir('src/lib/sodium')
  subdir('src/jwt')
endif

subdir('src/net')
subdir('src/event')
subdir('src/event/net')
subdir('src/event/net/cares')
subdir('src/event/systemd')
subdir('src/event/uring')

subdir('src/lib/avahi')
if avahi_dep.found()
  subdir('demo/avahi')
endif

subdir('src/lib/curl')
subdir('src/lib/zlib')
subdir('src/lib/pcre')
subdir('src/pg')
subdir('src/lib/dbus')
subdir('src/lib/mariadb')
subdir('src/spawn')
subdir('src/stock')
subdir('src/translation')
subdir('src/was')
subdir('src/was/json')
subdir('src/was/async')

subdir('src/lua/net')
subdir('src/lua/event')
subdir('src/lua/pg')
subdir('src/lua/mariadb')

if compiler.get_id() != 'gcc' or compiler.version().version_compare('>=8')
  subdir('src/translation/server')
endif

subdir('test')
