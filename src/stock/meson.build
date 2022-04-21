stock = static_library(
  'stock',
  'Item.cxx',
  'AbstractStock.cxx',
  'Stock.cxx',
  'MapStock.cxx',
  'MultiStock.cxx',
  include_directories: inc,
)

stock_dep = declare_dependency(
  link_with: stock,
  dependencies: [event_dep],
)
