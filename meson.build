project('CM4all libcommon', ['c', 'cpp'], version: '0.1',
  default_options: [
    'c_std=c99',
    'cpp_std=c++17'
  ],
)

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
  '-DPACKAGE="' + meson.project_name() + '"',
  '-DVERSION="' + meson.project_version() + '"',
  '-Wall',
  '-Wextra',
  '-Wwrite-strings', '-Wcast-qual', '-Wcast-align', '-Wfloat-equal',
  '-Wshadow', '-Wpointer-arith', '-Wsign-compare',
  '-Wmissing-declarations', '-Wmissing-noreturn', '-Wmissing-format-attribute',
  '-Wredundant-decls', '-Wno-long-long', '-Wundef',
  '-Wunused',
  '-Wundef',
]

if debug
  common_flags += ['-DPOISON']
endif

test_cxxflags = [
  '-Wno-non-virtual-dtor',

  # work around bogus GCC7 warning "mangled name for ... will change
  # in C++17 because the exception specification is part of a function
  # type"
  '-Wno-noexcept-type',

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

inc = include_directories('src', 'fake')

subdir('src/util')
subdir('src/time')
subdir('src/http')
subdir('src/adata')
subdir('src/lua')
subdir('src/io')
subdir('src/system')
subdir('src/net')
subdir('src/event')
subdir('src/event/net')
subdir('src/event/net/cares')
subdir('src/avahi')
subdir('src/curl')
subdir('src/zlib')
subdir('src/pg')
subdir('src/odbus')
subdir('src/spawn')
subdir('src/translation')
subdir('src/translation/server')

subdir('test')
