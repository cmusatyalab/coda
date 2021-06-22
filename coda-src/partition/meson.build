partition_sources = [
  'backupifs.c',
  'ftreeifs.c',
  'ftreeifs.h',
  'inodeops.c',
  'inodeops.h',
  'partition.c',
  'partition.h',
  'simpleifs.c',
  'simpleifs.h',
  'viceinode.h',
  'vicetab.c',
  'vicetab.h',
]

partition_lib = static_library('partition',
  partition_sources,
  build_by_default : false,
  include_directories : [config_h_include, base_inc, util_inc, vicedep_inc],
  dependencies : [rpc2_dep],
)

partition_inc = include_directories('.')

partition_dep = declare_dependency(
  include_directories : partition_inc,
  link_with : partition_lib,
)

if not get_option('build_server').disabled()
  inoder_sources = ['inoder.c']
  inoder = executable('inoder',
    inoder_sources,
    include_directories : [config_h_include, base_inc, util_inc, vicedep_inc],
    dependencies : [util_dep, base_dep, rpc2_dep],
    link_with : [partition_lib],
    install : true,
    install_dir : get_option('sbindir'),
  )

  install_man('vicetab.5')
endif
