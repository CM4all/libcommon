libwas = dependency('libcm4all-was-simple', version: '>= 1.9')

was = static_library(
  'was',
  'WasOutputStream.cxx',
  'WasSplice.cxx',
  include_directories: inc,
  dependencies: [
    libwas,
  ],
)

was_dep = declare_dependency(
  link_with: was,
  dependencies: [
    libwas,
  ],
)
