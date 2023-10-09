if not avahi_dep.found()
  subdir_done()
endif

executable(
  'RunAvahiExplorer',
  'RunAvahiExplorer.cxx',
  include_directories: inc,
  dependencies: [
    avahi_dep,
    net_dep,
    io_dep,
  ],
)
