libwas = dependency('libcm4all-was-simple', version: '>= 1.9', required: get_variable('libcommon_enable_was', true))
if not libwas.found()
  was_dep = disabler()
  subdir_done()
endif

was = static_library(
  'was',
  'ExpectRequestBody.cxx',
  'FormRequestBody.cxx',
  'SimpleResponse.cxx',
  'StringRequestBody.cxx',
  'Reader.cxx',
  'WasOutputStream.cxx',
  'Splice.cxx',
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
