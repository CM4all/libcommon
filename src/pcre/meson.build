libpcre = dependency('libpcre')

pcre = static_library(
  'pcre',
  'Regex.cxx',
  include_directories: inc,
  dependencies: [
    libpcre,
  ],
)

pcre_dep = declare_dependency(
  link_with: pcre,
)
