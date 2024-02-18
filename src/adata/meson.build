adata = static_library(
  'adata',
  'ExpandableStringList.cxx',
  'AssignmentList.cxx',
  include_directories: inc,
  dependencies: alloc_dep,
)

adata_dep = declare_dependency(
  link_with: adata,
)
