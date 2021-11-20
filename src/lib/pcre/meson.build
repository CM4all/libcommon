libpcre = dependency('libpcre2-8')

libpcre = declare_dependency(
  compile_args: '-DPCRE2_CODE_UNIT_WIDTH=0',
  dependencies: libpcre,
)

pcre = static_library(
  'pcre',
  'Error.cxx',
  'UniqueRegex.cxx',
  include_directories: inc,
  dependencies: [
    libpcre,
  ],
)

pcre_dep = declare_dependency(
  link_with: pcre,
  dependencies: [
    libpcre,
  ],
)
