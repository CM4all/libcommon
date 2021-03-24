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
  'Config.cxx',
  'Init.cxx',
  'NetworkNamespace.cxx',
  'PidNamespace.cxx',
  'Registry.cxx',
  'ResourceLimits.cxx',
  'SeccompFilter.cxx',
  'SyscallFilter.cxx',
  'UidGid.cxx',
  'UserNamespace.cxx',
  'daemon/Client.cxx',
]

if compiler.get_id() != 'gcc' or compiler.version().version_compare('>=8')
  # the C++17 Filesystem TS is implemented in GCC>=8
  spawn_sources += 'ConfigParser.cxx'
endif

spawn_dependencies = [
]

if get_variable('libcommon_enable_AllocatorPtr', true)
  spawn_sources += [
    'CgroupOptions.cxx',
    'CgroupKill.cxx',
    'CgroupState.cxx',
    'CgroupWatch.cxx',
    'ChildOptions.cxx',
    'Client.cxx',
    'Direct.cxx',
    'Glue.cxx',
    'Interface.cxx',
    'Launch.cxx',
    'Local.cxx',
    'Mount.cxx',
    'MountNamespaceOptions.cxx',
    'NamespaceOptions.cxx',
    'Prepared.cxx',
    'Server.cxx',
    'VfsBuilder.cxx',
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
