gtest_main_dep = dependency('gtest', main: true,
                            required: get_option('test'),
                            fallback: ['gtest', 'gtest_main_dep'])
if not gtest_main_dep.found()
  subdir_done()
endif

gtest = declare_dependency(
  dependencies: [gtest_main_dep],
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
