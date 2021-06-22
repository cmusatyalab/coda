dir_sources = [
  'codadir.c',
  'codadir.h',
  'dhcache.c',
  'dirbody.c',
  'dirbody.h',
  'dirinode.c',
  'fid.c',
]

dir_lib = static_library('codadir',
  dir_sources,
  build_by_default : false,
  include_directories : [config_h_include, base_inc, kerndep_inc, util_inc,
                         vicedep_inc],
  dependencies : [rvm_rpc2_dep],
)

dir_inc = include_directories('.')

dir_dep = declare_dependency(
  include_directories : dir_inc,
  link_with : dir_lib,
)
