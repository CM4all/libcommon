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

if compiler.get_id() == 'clang'
  gtest_main_dep = gtest_sub.get_variable('gtest_main_dep')
else
  gtest_main_dep = dependency('gtest', main: true)
endif

gtest = declare_dependency(
  dependencies: [gtest_main_dep],
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
subdir('sodium')
subdir('spawn')
subdir('time')
subdir('uring')

if get_option('coroutines')
  subdir('co')
endif
