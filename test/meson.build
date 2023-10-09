gtest_main_dep = dependency('gtest', main: true,
                            required: get_option('test'),
                            fallback: ['gtest', 'gtest_main_dep'])
if not gtest_main_dep.found()
  subdir_done()
endif

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
  dependencies: [gtest_main_dep],
  compile_args: gtest_warning_flags + [
    '-Wno-undef',
    '-Wno-missing-declarations',
  ],
)

subdir('util')
subdir('uri')
subdir('http')
subdir('io/config')
subdir('net')
subdir('pcre')
subdir('pg')
subdir('nettle')
subdir('sodium')
subdir('jwt')
subdir('ssl')
subdir('stock')
subdir('time')
subdir('co')
subdir('lua')
