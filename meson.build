project('CM4all libcommon', ['c', 'cpp'], version: '0.1',
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

# TODO: use get_option('b_ndebug'), but that doesn't work with Meson 0.37.1
if get_option('buildtype') == 'debug'
  debug = true
else
  debug = false
endif

compiler = meson.get_compiler('cpp')
c_compiler = meson.get_compiler('c')

common_flags = [
  '-D_REENTRANT', '-D_GNU_SOURCE',
  '-Wwrite-strings', '-Wcast-qual', '-Wcast-align', '-Wfloat-equal',
  '-Wshadow', '-Wpointer-arith', '-Wsign-compare',
  '-Wmissing-declarations', '-Wmissing-noreturn', '-Wmissing-format-attribute',
  '-Wredundant-decls', '-Wno-long-long',
  '-Wunused',
  '-Wundef',
  '-Wvla',
]

if debug
  common_flags += ['-DPOISON']
endif

test_cxxflags = [
  '-Wno-non-virtual-dtor',

  # the only warnings we got from this are from formatted error
  # messages, and their truncation is harmless
  '-Wno-format-truncation',
]

test_cflags = [
  '-Wmissing-prototypes', '-Wstrict-prototypes',
  '-Wbad-function-cast',
  '-Waggregate-return',
  '-Wnested-externs',
  '-pedantic',
]

add_global_arguments(common_flags, language: 'c')
add_global_arguments(common_flags, language: 'cpp')

foreach f: test_cxxflags
  if compiler.has_argument(f)
    add_global_arguments(f, language: 'cpp')
  endif
endforeach

foreach f: test_cflags
  if c_compiler.has_argument(f)
    add_global_arguments(f, language: 'c')
  endif
endforeach

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
subdir('src/curl')
subdir('src/zlib')
subdir('src/pcre')
subdir('src/pg')
subdir('src/odbus')
subdir('src/spawn')
subdir('src/stock')
subdir('src/translation')
subdir('src/was')

if compiler.get_id() != 'gcc' or compiler.version().version_compare('>=8')
  subdir('src/translation/server')
endif

subdir('test')
