gtest = dependency('gtest', main: true,
                   include_type: 'system',
                   required: get_option('test'),
                   fallback: ['gtest', 'gtest_main_dep'])
if not gtest.found()
  subdir_done()
endif

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
