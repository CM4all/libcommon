if not libwas_protocol.found()
  subdir_done()
endif

test(
  'TestWas',
  executable(
    'TestWas',
    'TestSimpleClient.cxx',
    'TestSimpleServer.cxx',
    'TestSimpleServerProtocol.cxx',
    include_directories: inc,
    dependencies: [
      gtest,
      was_server_async_dep,
    ],
  ),
)
