stock = static_library(
  'stock',
  'Item.cxx',
  'AbstractStock.cxx',
  'BasicStock.cxx',
  'Stock.cxx',
  'MapStock.cxx',
  'MultiStock.cxx',
  include_directories: inc,
  dependencies: [
    event_dep,
    util_dep,
  ],
)

stock_dep = declare_dependency(
  link_with: stock,
  dependencies: [
    event_dep,
    util_dep,
  ],
)
