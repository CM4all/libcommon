stock = static_library(
  'stock',
  'Item.cxx',
  'Stock.cxx',
  'MapStock.cxx',
  include_directories: inc,
)

stock_dep = declare_dependency(
  link_with: stock,
  dependencies: [event_dep],
)
