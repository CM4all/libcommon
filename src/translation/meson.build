translation = static_library(
  'translation',
  'PReader.cxx',
  'Parser.cxx',
  'Response.cxx',
  include_directories: inc,
  dependencies: [
    # using get_variable() because this library can be built without
    # PCRE support, but Meson can't check the value of the
    # TRANSLATION_ENABLE_EXPAND macro of translation/Features.h
    get_variable('pcre_dep', []),

    fmt_dep,
  ],
)

translation_dep = declare_dependency(
  link_with: translation,
  dependencies: [
    spawn_dep,
    net_dep,
  ],
)
