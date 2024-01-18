project('CM4all libcommon', 'cpp', version: '0.1',
  meson_version: '>= 0.56',
  default_options: [
    'cpp_std=c++2a',
    'warning_level=3',

    'libwas:apreq2=disabled',
    'libwas:xios=disabled',
    'libwas:documentation=disabled',
  ],
)

version_h = configuration_data()
version_h.set_quoted('PACKAGE', meson.project_name())
version_h.set_quoted('VERSION', meson.project_version())
configure_file(output: 'version.h', configuration: version_h)

debug = get_option('b_ndebug') != 'true'

compiler = meson.get_compiler('cpp')

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

add_global_arguments(common_flags, language: 'cpp')
add_global_arguments(compiler.get_supported_arguments(test_global_cxxflags), language: 'cpp')
add_project_arguments(compiler.get_supported_arguments(test_cxxflags), language: 'cpp')

if get_option('fuzzer')
  fuzzer_flags = ['-fsanitize=fuzzer']
  add_global_arguments(fuzzer_flags, language: 'cpp')
  add_global_link_arguments(fuzzer_flags, language: 'cpp')
endif

if compiler.has_header('valgrind/memcheck.h')
  add_global_arguments('-DHAVE_VALGRIND_MEMCHECK_H', language: 'cpp')
endif

inc = include_directories('src', 'fake', '.')

libcommon_require_avahi = get_option('avahi')
libcommon_require_cap = get_option('cap')
libcommon_require_curl = get_option('curl')
libcommon_require_lua = get_option('lua')
libcommon_require_mariadb = get_option('mariadb')
libcommon_require_nettle = get_option('nettle')
libcommon_require_pcre = get_option('pcre')
libcommon_require_pg = get_option('pg')
libcommon_require_sodium = get_option('sodium')
libcommon_require_uring = get_option('uring')
libcommon_enable_was = get_option('was')
libcommon_enable_json = get_option('json')
libcommon_enable_seccomp = get_option('seccomp')

subdir('src/util')
subdir('src/lib/fmt')
subdir('src/uri')
subdir('src/time')
subdir('src/http')
subdir('src/co')

subdir('src/adata')
subdir('src/lua')
subdir('src/io')
subdir('src/io/config')
subdir('src/io/linux')
subdir('src/io/uring')
subdir('src/system')
subdir('src/memory')
subdir('src/lib/openssl')

if lua_dep.found()
  subdir('src/lua/io')
endif

subdir('src/lib/nettle')
subdir('src/lib/nlohmann_json')
subdir('src/lib/sodium')
subdir('src/jwt')

if lua_dep.found() and nlohmann_json_dep.found()
  subdir('src/lua/json')
endif

subdir('src/net')
subdir('src/net/control')
subdir('src/net/djb')
subdir('src/net/log')

if not get_option('fuzzer')
  subdir('src/event')
  subdir('src/event/net')
  subdir('src/event/net/control')
  subdir('src/event/net/djb')
  subdir('src/event/net/log')
  subdir('src/event/systemd')
  subdir('src/event/uring')
  subdir('src/thread')

  subdir('src/lib/avahi')
  subdir('src/lib/cap')
  subdir('src/lib/curl')
  subdir('src/lib/zlib')
  subdir('src/lib/pcre')
  subdir('src/stock')
  subdir('src/pg')
  subdir('src/lib/dbus')
  subdir('src/lib/mariadb')

  subdir('src/spawn')
  subdir('src/translation')
  subdir('src/translation/server')

  subdir('src/was')
  subdir('src/was/async')

  if nlohmann_json_dep.found()
    subdir('src/was/nlohmann_json')
  endif
endif

if lua_dep.found() and not get_option('fuzzer')
  subdir('src/lua/net')
  subdir('src/lua/event')
  subdir('src/lua/sodium')
  subdir('src/lua/pg')
  subdir('src/lua/mariadb')
endif

if get_option('fuzzer')
  subdir('fuzzer')
else
  subdir('test')
  subdir('demo')
endif
