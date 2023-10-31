if not libsystemd.found()
  subdir_done()
endif

if nlohmann_json.found()
  executable(
    'RunResolvedClient',
    'RunResolvedClient.cxx',
    include_directories: inc,
    dependencies: [
      event_systemd_dep,
      fmt_dep,
    ],
  )
endif

