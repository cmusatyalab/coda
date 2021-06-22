rwcdb_sources = [
  'rwcdb.c',
  'rwcdb.h',
  'rwcdb_file.c',
  'rwcdb_file.h',
  'rwcdb_pack.h',
]

rwcdb_lib = static_library('rwcdb',
  rwcdb_sources,
  include_directories : [config_h_include],
  dependencies : [base_dep],
)

rwcdb_dep = declare_dependency(
  include_directories : [include_directories('.')],
  link_with : rwcdb_lib,
)
