libpcre = dependency('libpcre')

pcre = static_library(
  'pcre',
  'UniqueRegex.cxx',
  include_directories: inc,
  dependencies: [
    libpcre,
  ],
)

pcre_dep = declare_dependency(
  link_with: pcre,
)
