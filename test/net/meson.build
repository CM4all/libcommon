test_net_sources = []
test_net_dependencies = []

if is_variable('net_log_dep')
  test_net_sources += 'TestLog.cxx'
  test_net_dependencies += net_log_dep
endif


test(
  'TestNet',
  executable(
    'TestNet',
    'TestAnonymize.cxx',
    'TestIPv4Address.cxx',
    'TestIPv6Address.cxx',
    'TestLiterals.cxx',
    'TestLocalSocketAddress.cxx',
    'TestHostParser.cxx',
    'TestFormatAddress.cxx',
    'TestMaskedSocketAddress.cxx',
    'TestEasyMessage.cxx',
    test_net_sources,
    include_directories: inc,
    dependencies: [gtest, net_dep] + test_net_dependencies,
  ),
)
