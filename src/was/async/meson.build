libwas_protocol = dependency('libcm4all-was-protocol')

was_async = static_library(
  'was_async',
  'Control.cxx',
  'Socket.cxx',
  include_directories: inc,
  dependencies: [
    libwas_protocol,
  ],
)

was_async_dep = declare_dependency(
  link_with: was_async,
  dependencies: [
    libwas_protocol,
  ],
)
