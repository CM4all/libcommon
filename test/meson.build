gtest_warning_flags = []
if compiler.get_id() == 'gcc'
  gtest_warning_flags += [
    '-Wno-suggest-attribute=format',
    '-Wno-suggest-attribute=noreturn',
  ]
endif

gtest = declare_dependency(
  dependencies: [dependency('gtest', main: true)],
  compile_args: gtest_warning_flags + [
    '-Wno-undef',
    '-Wno-missing-declarations',

    # needed on Jessie for gtest's IsNullLiteralHelper
    '-Wno-conversion-null',
  ],
)

subdir('util')
subdir('http')
subdir('io')
subdir('net')
subdir('pg')
subdir('cares')
subdir('spawn')
subdir('time')
