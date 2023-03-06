libpcre = dependency('libpcre2-8',
                     required: get_variable('libcommon_require_pcre', true))
if not libpcre.found()
  pcre_dep = libpcre
  subdir_done()
endif

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
    fmt_dep,
  ],
)

pcre_dep = declare_dependency(
  link_with: pcre,
  dependencies: [
    libpcre,
  ],
)
