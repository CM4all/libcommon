spawn_config = configuration_data()
spawn_config.set('HAVE_LIBCAP', cap_dep.found())

spawn_options_sources = [
  'CgroupState.cxx',
]

spawn_options_dependencies = [
]

spawn_sources = [
  'CgroupKill.cxx',
  'ZombieReaper.cxx',
]

spawn_internal_dependencies = [
]

spawn_dependencies = [
]

if get_variable('libcommon_enable_spawn_server', true)
  # Enable the dedicated spawner process with client/server protocol
  # (IProtocol.hxx) between original process and spawner process.

  spawn_sources += [
    'Client.cxx',
    'CgroupMemoryWatch.cxx',
    'CgroupMultiWatch.cxx',
    'CgroupPidsWatch.cxx',
    'Launch.cxx',
    'Server.cxx',
    'TmpfsManager.cxx',
  ]

  libcommon_enable_spawn_registry = true
  libcommon_enable_spawn_direct = true
  libcommon_enable_spawn_config = true

  if get_variable('libcommon_enable_libsystemd', true)
    spawn_sources += [
      'Systemd.cxx',
      'ScopeProcess.cxx',
    ]

    libsystemd = dependency('libsystemd')
    spawn_config.set('HAVE_LIBSYSTEMD', libsystemd.found())

    spawn_internal_dependencies += [
      libsystemd,
      odbus_dep,
    ]
  endif
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

  spawn_internal_dependencies += [
    time_dep,
  ]

  libcommon_enable_spawn_options = true
endif

if get_variable('libcommon_enable_spawn_direct', false) or get_variable('libcommon_enable_spawn_init', false)
  libseccomp = dependency('libseccomp',
                          required: get_variable('libcommon_enable_seccomp', true))
  spawn_config.set('HAVE_LIBSECCOMP', libseccomp.found())
  if libseccomp.found()
    spawn_internal_dependencies += libseccomp
    spawn_sources += 'SeccompFilter.cxx'
  endif
endif

if get_variable('libcommon_enable_spawn_direct', false)
  # Enable the SpawnChildProcess() which spawns the
  # child process directly (without the #SpawnService interface).

  spawn_sources += [
    'Direct.cxx',
    'ErrorPipe.cxx',
  ]

  if libseccomp.found()
    spawn_sources += 'SyscallFilter.cxx'
  endif

  libcommon_enable_spawn_daemon = true
  libcommon_enable_spawn_init = true
  libcommon_enable_spawn_options = true
endif

if get_variable('libcommon_enable_spawn_daemon', false)
  # Enable the client implementation for the spawn daemon (package
  # "cm4all-spawn").

  spawn_options_sources += [
    'accessory/Client.cxx',
  ]
endif

if get_variable('libcommon_enable_spawn_init', false)
  # Enable the "init" implementation (for PID namespaces).

  spawn_sources += [
    'Init.cxx',
  ]
endif

if get_variable('libcommon_enable_spawn_options', false)
  # Enable the structs ChildOptions and PreparedChildProcess (which in
  # turn need an AllocatorPtr implementation).

  spawn_options_sources += [
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
    'VfsBuilder.cxx',
  ]

  spawn_options_dependencies += [
    alloc_dep,
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

configure_file(output: 'config.h', configuration: spawn_config)

spawn_options = static_library(
  'spawn_options',
  spawn_options_sources,
  include_directories: inc,
  dependencies: [
    util_dep,
    io_linux_dep,
    cap_dep,
    fmt_dep,
  ],
)

spawn_options_dep = declare_dependency(
  link_with: spawn_options,
  dependencies: spawn_options_dependencies + [
    system_dep,
    io_dep,
    net_dep,
  ],
)

spawn = static_library(
  'spawn',
  spawn_sources,
  include_directories: inc,
  dependencies: spawn_internal_dependencies + [
    spawn_options_dep,
    util_dep,
    io_linux_dep,
    cap_dep,
    fmt_dep,
  ],
)

spawn_dep = declare_dependency(
  link_with: spawn,
  dependencies: spawn_dependencies + [
    system_dep,
    io_dep,
    net_dep,
  ],
)
