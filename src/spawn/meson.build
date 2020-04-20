if get_variable('libcommon_enable_libsystemd', true)
  libsystemd = dependency('libsystemd')
  libsystemd = declare_dependency(
    compile_args: ['-DHAVE_LIBSYSTEMD'],
    dependencies: [
      libsystemd,
      odbus_dep,
    ],
  )
else
  libsystemd = dependency('', required: false)
endif

libseccomp = dependency('libseccomp')

spawn_sources = [
  'SeccompFilter.cxx',
  'SyscallFilter.cxx',
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
    'CgroupWatch.cxx',
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

if libsystemd.found()
  spawn_sources += [
    'Systemd.cxx',
  ]
endif

spawn = static_library(
  'spawn',
  spawn_sources,
  include_directories: inc,
  dependencies: [
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
    libsystemd,
  ],
)
