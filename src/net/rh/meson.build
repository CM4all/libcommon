net_rh = static_library(
  'net_rh',
  'Node.cxx',
  include_directories: inc,
  dependencies: [
    net_dep,
    util_dep,
    avahi_dep,
  ],
)

net_rh_dep = declare_dependency(
  link_with: net_rh,
  dependencies: [
    net_dep,
  ],
)
