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
  'Config.cxx',
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

if compiler.get_id() != 'gcc' or compiler.version().version_compare('>=8')
  # the C++17 Filesystem TS is implemented in GCC>=8
  spawn_sources += 'ConfigParser.cxx'
endif

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
endif

if get_variable('libcommon_enable_spawn_local', true)
  # Enable the "LocalSpawnService" class (which nobody really uses
  # anymore, and remains here only for debugging purposes).

  spawn_sources += [
    'Local.cxx',
  ]

  libcommon_enable_spawn_direct = true
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
    io_config_dep,
    net_dep,
    time_dep,
    libsystemd,
  ],
)
