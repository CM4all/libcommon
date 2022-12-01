libfmt = dependency('fmt', version: '>= 7', fallback: ['fmt', 'fmt_dep'])

fmt = static_library(
  'fmt',
  'RuntimeError.cxx',
  'SystemError.cxx',
  include_directories: inc,
  dependencies: libfmt,
)

fmt_dep = declare_dependency(
  link_with: fmt,
  dependencies: libfmt,
)
