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
  'CgroupKill.cxx',
  'CgroupState.cxx',
  'CgroupWatch.cxx',
  'Init.cxx',
  'NetworkNamespace.cxx',
  'PidNamespace.cxx',
  'PidfdEvent.cxx',
  'Registry.cxx',
  'ResourceLimits.cxx',
  'SeccompFilter.cxx',
  'SyscallFilter.cxx',
  'UidGid.cxx',
  'UserNamespace.cxx',
  'VfsBuilder.cxx',
  'ZombieReaper.cxx',
  'daemon/Client.cxx',
]

spawn_dependencies = [
]

if get_variable('libcommon_enable_spawn_server', true)
  # Enable the dedicated spawner process with client/server protocol
  # (IProtocol.hxx) between original process and spawner process.

  spawn_sources += [
    'Client.cxx',
    'Launch.cxx',
    'Server.cxx',
  ]

  libcommon_enable_spawn_direct = true
  libcommon_enable_spawn_config = true
endif

if get_variable('libcommon_enable_spawn_local', true)
  # Enable the "LocalSpawnService" class (which nobody really uses
  # anymore, and remains here only for debugging purposes).

  spawn_sources += [
    'Local.cxx',
  ]

  libcommon_enable_spawn_direct = true
  libcommon_enable_spawn_config = true
endif

if get_variable('libcommon_enable_spawn_direct', false)
  # Enable the SpawnChildProcess() which spawns the
  # child process directly (without the #SpawnService interface).

  spawn_sources += [
    'Direct.cxx',
  ]

  libcommon_enable_spawn_options = true
endif

if get_variable('libcommon_enable_spawn_options', false)
  # Enable the structs ChildOptions and PreparedChildProcess (which in
  # turn need an AllocatorPtr implementation).

  spawn_sources += [
    'CgroupOptions.cxx',
    'ChildOptions.cxx',
    'Mount.cxx',
    'MountNamespaceOptions.cxx',
    'NamespaceOptions.cxx',
    'Prepared.cxx',
  ]

  spawn_dependencies += [
    adata_dep,
  ]
endif

if get_variable('libcommon_enable_spawn_config', false)
  spawn_sources += [
    'Config.cxx',
    'ConfigParser.cxx',
  ]

  spawn_dependencies += [
    io_config_dep,
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
    util_dep,
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
    time_dep,
    libsystemd,
  ],
)
