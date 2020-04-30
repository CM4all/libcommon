gtest_warning_flags = []
if compiler.get_id() == 'gcc'
  gtest_warning_flags += [
    '-Wno-suggest-attribute=format',
    '-Wno-suggest-attribute=noreturn',
  ]
endif

if compiler.get_id() == 'clang' and compiler.version().version_compare('>=10')
  gtest_warning_flags += [
    # work around clang warning caused by GTest's wrong "-lpthread"
    # compiler flag
    '-Wno-unused-command-line-argument',
  ]
endif

gtest = declare_dependency(
  dependencies: [dependency('gtest', main: true)],
  compile_args: gtest_warning_flags + [
    '-Wno-undef',
    '-Wno-missing-declarations',
  ],
)

subdir('util')
subdir('http')
subdir('io')
subdir('net')
subdir('pcre')
subdir('pg')
subdir('cares')
subdir('spawn')
subdir('time')
subdir('uring')
