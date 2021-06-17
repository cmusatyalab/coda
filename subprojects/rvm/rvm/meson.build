rvm_sources = [
  'rvm_debug.c',
  'rvm_init.c',
  'rvm_io.c',
  'rvm_logflush.c',
  'rvm_logrecovr.c',
  'rvm_logstatus.c',
  'rvm_map.c',
  'rvm_printers.c',
  'rvm_private.h',
  'rvm_status.c',
  'rvm_trans.c',
  'rvm_unmap.c',
  'rvm_utils.c',
]

rvm_lib = library('rvm',
  rvm_sources,
  version : lt_version,
  include_directories : [config_h_include, rvm_inc],
  install : true,
)

rvm_dep = declare_dependency(
  include_directories : [rvm_inc],
  link_with : rvm_lib,
)

rvmlwp_lib = library('rvmlwp',
  rvm_sources,
  version : lt_version,
  include_directories : [config_h_include, rvm_inc],
  c_args : ['-DRVM_USELWP'],
  dependencies : [lwp_dep],
  install : true,
)

rvmlwp_dep = declare_dependency(
  include_directories : [rvm_inc],
  dependencies : [lwp_dep],
  link_with : rvmlwp_lib,
)

rvmpt_lib = library('rvmpt',
  [rvm_sources, 'rvm_pthread.c'],
  version : lt_version,
  include_directories : [config_h_include, rvm_inc],
  c_args : ['-DRVM_USEPT'],
  dependencies : [pthread_dep],
  install : true,
)

rvmpt_dep = declare_dependency(
  include_directories : rvm_inc,
  dependencies : [pthread_dep],
  link_with : rvmpt_lib,
)

rvmutl = executable('rvmutl', 'rvmutl.c',
  include_directories : [config_h_include],
  dependencies : [rvm_dep],
  install : get_option('install-tools'),
  install_dir : get_option('sbindir'),
)
install_man('rvmutl.1')
