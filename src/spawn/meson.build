libsystemd = dependency('libsystemd')
libseccomp = dependency('libseccomp')

spawn_sources = [
  'SeccompFilter.cxx',
  'SyscallFilter.cxx',
  'Systemd.cxx',
  'Registry.cxx',
  'Init.cxx',
  'UserNamespace.cxx',
  'PidNamespace.cxx',
  'NetworkNamespace.cxx',
  'UidGid.cxx',
  'ResourceLimits.cxx',
  'ConfigParser.cxx',
  'daemon/Client.cxx',
]

spawn_dependencies = [
]

if get_variable('libcommon_enable_AllocatorPtr', true)
  spawn_sources += [
    'Prepared.cxx',
    'Direct.cxx',
    'Interface.cxx',
    'Local.cxx',
    'MountNamespaceOptions.cxx',
    'MountList.cxx',
    'NamespaceOptions.cxx',
    'JailConfig.cxx',
    'JailParams.cxx',
    'ChildOptions.cxx',
    'CgroupOptions.cxx',
    'CgroupKill.cxx',
    'RefenceOptions.cxx',
    'Server.cxx',
    'Launch.cxx',
    'Client.cxx',
    'Glue.cxx',
  ]

  spawn_dependencies += [
    adata_dep,
  ]
endif

spawn = static_library(
  'spawn',
  spawn_sources,
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
  dependencies: spawn_dependencies + [
    system_dep,
    io_dep,
    net_dep,
    odbus_dep,
  ],
)
