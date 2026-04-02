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

executable(
  'RunRendezvousHash',
  'RunRendezvousHash.cxx',
  include_directories: inc,
  dependencies: [
    fmt_dep,
    avahi_dep,
    net_rh_dep,
    net_dep,
    io_dep,
  ],
)
