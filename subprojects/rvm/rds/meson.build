rds_sources = [
  'rds_coalesce.c',
  'rds_free.c',
  'rds_init.c',
  'rds_malloc.c',
  'rds_maxblock.c',
  'rds_prealloc.c',
  'rds_private.h',
  'rds_split.c',
  'rds_start.c',
  'rds_stats.c',
  'rds_util.c',
  'rds_zap.c',
]

rds_lib = library('rds',
  rds_sources,
  version : lt_version,
  include_directories : [config_h_include, rvm_inc],
  dependencies : [seg_dep],
  install : get_option('install-dev'),
)

rds_dep = declare_dependency(
  include_directories : [rvm_inc],
  dependencies : [seg_dep],
  link_with : rds_lib,
)

rdslwp_lib = library('rdslwp',
  rds_sources,
  version : lt_version,
  include_directories : [config_h_include, rvm_inc],
  c_args : ['-DRVM_USELWP'],
  dependencies : [seglwp_dep],
  install : true,
)

rdslwp_dep = declare_dependency(
  include_directories : [rvm_inc],
  dependencies : [seglwp_dep],
  link_with : rdslwp_lib,
)

rdspt_lib = library('rdspt',
  rds_sources,
  version : lt_version,
  include_directories : [config_h_include, rvm_inc],
  c_args : ['-DRVM_USEPT'],
  dependencies : [segpt_dep],
  install : true,
)

rdspt_dep = declare_dependency(
  include_directories : [rvm_inc],
  dependencies : [segpt_dep],
  link_with : rdspt_lib,
)

rdsinit = executable('rdsinit', 'rdsinit.c',
  include_directories : [config_h_include],
  dependencies : [rds_dep],
  install : get_option('install-tools'),
  install_dir : get_option('sbindir'),
)
install_man('rdsinit.1')
