stock_sources = [
  'Item.cxx',
  'AbstractStock.cxx',
  'Stock.cxx',
]

if get_variable('libcommon_enable_boost', true)
  stock_sources += [
    'MapStock.cxx',
    'MultiStock.cxx',
  ]
endif

stock = static_library(
  'stock',
  stock_sources,
  include_directories: inc,
)

stock_dep = declare_dependency(
  link_with: stock,
  dependencies: [event_dep],
)
