libsystemd = dependency('libsystemd')
libseccomp = dependency('libseccomp')

spawn = static_library(
  'spawn',
  'SeccompFilter.cxx',
  'SyscallFilter.cxx',
  'Systemd.cxx',
  'Prepared.cxx',
  'Registry.cxx',
  'Init.cxx',
  'Direct.cxx',
  'Interface.cxx',
  'Local.cxx',
  'UserNamespace.cxx',
  'PidNamespace.cxx',
  'NetworkNamespace.cxx',
  'NamespaceOptions.cxx',
  'MountNamespaceOptions.cxx',
  'MountList.cxx',
  'JailConfig.cxx',
  'JailParams.cxx',
  'ChildOptions.cxx',
  'CgroupOptions.cxx',
  'UidGid.cxx',
  'ResourceLimits.cxx',
  'RefenceOptions.cxx',
  'Server.cxx',
  'Launch.cxx',
  'Client.cxx',
  'Glue.cxx',
  'ConfigParser.cxx',
  'daemon/Client.cxx',
  include_directories: inc,
  dependencies: [
    libdbus,
    libcap,
    libseccomp,
    libsystemd,
  ],
)

spawn_dep = declare_dependency(
  link_with: spawn,
  dependencies: [
    system_dep,
    adata_dep,
    io_dep,
    net_dep,
    odbus_dep,
  ],
)
