project('CM4all libcommon', 'cpp', version: '0.1',
  default_options : ['cpp_std=c++14'])

add_global_arguments('-D_REENTRANT', '-D_GNU_SOURCE',
  '-Wall',
  '-Wextra',
  '-Wwrite-strings', '-Wcast-qual', '-Wcast-align', '-Wfloat-equal',
  '-Wshadow', '-Wpointer-arith', '-Wsign-compare',
  '-Wmissing-declarations', '-Wmissing-noreturn', '-Wmissing-format-attribute',
  '-Wredundant-decls', '-Wno-long-long', '-Wundef',
  '-Wunused',
  '-pedantic',
  '-Wno-non-virtual-dtor',
  '-Werror', '-pedantic-errors',
  language: 'cpp')

boost = dependency('boost', version: '>= 1.54')
libevent = dependency('libevent', version: '>= 2.0.19')
libsystemd = dependency('libsystemd')
libinline = dependency('libcm4all-inline', version: '>= 0.3.4')
libsocket = dependency('libcm4all-socket', version: '>= 0.3.5')

inc = include_directories('src')

util = static_library('util',
  'src/util/AllocatedString.cxx',
  'src/util/Exception.cxx',
  'src/util/LeakDetector.cxx',
  'src/util/PrintException.cxx',
  'src/util/StringBuilder.cxx',
  'src/util/StringParser.cxx',
  'src/util/StringUtil.cxx',
  'src/util/StringView.cxx',
  include_directories: inc,
  dependencies: [
    libinline,
  ])
util_dep = declare_dependency(link_with: util)
