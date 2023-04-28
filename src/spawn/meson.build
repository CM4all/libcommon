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

spawn_sources = [
  'CgroupKill.cxx',
  'CgroupState.cxx',
  'ZombieReaper.cxx',
]

spawn_dependencies = [
]

if get_variable('libcommon_enable_spawn_server', true)
  # Enable the dedicated spawner process with client/server protocol
  # (IProtocol.hxx) between original process and spawner process.

  spawn_sources += [
    'Client.cxx',
    'CgroupWatch.cxx',
    'Launch.cxx',
    'Server.cxx',
    'TmpfsManager.cxx',
  ]

  libcommon_enable_spawn_registry = true
  libcommon_enable_spawn_direct = true
  libcommon_enable_spawn_config = true
endif

if get_variable('libcommon_enable_spawn_local', true)
  # Enable the "LocalSpawnService" class (which nobody really uses
  # anymore, and remains here only for debugging purposes).

  spawn_sources += [
    'Local.cxx',
  ]

  libcommon_enable_spawn_registry = true
  libcommon_enable_spawn_direct = true
  libcommon_enable_spawn_config = true
endif

if get_variable('libcommon_enable_spawn_registry', false)
  # Enable the ChildProcessRegistry class.

  spawn_sources += [
    'PidfdEvent.cxx',
    'Registry.cxx',
  ]

  spawn_dependencies += [
    time_dep,
  ]

  libcommon_enable_spawn_options = true
endif

if get_variable('libcommon_enable_spawn_direct', false)
  # Enable the SpawnChildProcess() which spawns the
  # child process directly (without the #SpawnService interface).

  spawn_sources += [
    'Direct.cxx',
    'ErrorPipe.cxx',
    'SyscallFilter.cxx',
  ]

  libcommon_enable_spawn_daemon = true
  libcommon_enable_spawn_init = true
  libcommon_enable_spawn_options = true
endif

if get_variable('libcommon_enable_spawn_daemon', false)
  # Enable the client implementation for the spawn daemon (package
  # "cm4all-spawn").

  spawn_sources += [
    'daemon/Client.cxx',
  ]
endif

if get_variable('libcommon_enable_spawn_init', false)
  # Enable the "init" implementation (for PID namespaces).

  spawn_sources += [
    'Init.cxx',
    'SeccompFilter.cxx',
  ]

  spawn_dependencies += [
    dependency('libseccomp')
  ]
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
    'TmpfsCreate.cxx',

    # These are really depended on up libcommon_enable_spawn_direct,
    # but the functions are called from ChildOptions/NamespaceOptions
    # etc.
    'NetworkNamespace.cxx',
    'PidNamespace.cxx',
    'ResourceLimits.cxx',
    'UidGid.cxx',
    'UserNamespace.cxx',
    'VfsBuilder.cxx',
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
    cap_dep,
    fmt_dep,
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
