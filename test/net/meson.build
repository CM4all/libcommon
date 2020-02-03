test_net_sources = []

if get_variable('libcommon_enable_net_log', true)
  test_net_sources += 'TestLog.cxx'
endif


test('TestNet', executable('TestNet',
  'TestAnonymize.cxx',
  'TestIPv4Address.cxx',
  'TestIPv6Address.cxx',
  'TestHostParser.cxx',
  'TestAddressString.cxx',
  'TestMaskedSocketAddress.cxx',
  test_net_sources,
  include_directories: inc,
  dependencies: [gtest, net_dep]))
