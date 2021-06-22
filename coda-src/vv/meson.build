vv_sources = [
  'inconsist.cc',
  'inconsist.h',
  'nettohost.cc',
  'nettohost.h',
]

vv_lib = static_library('vv',
  vv_sources,
  build_by_default : false,
  include_directories : [config_h_include, vicedep_inc],
  dependencies : [rvm_rpc2_dep, base_dep],
)

vv_inc = include_directories('.')

vv_dep = declare_dependency(
  include_directories : vv_inc,
  link_with : vv_lib,
)
