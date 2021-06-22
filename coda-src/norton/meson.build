norton_sources = [
  'commands.cc',
  'norton-dir.cc',
  'norton-rds.cc',
  'norton-recov.cc',
  'norton-setup.cc',
  'norton-vnode.cc',
  'norton-volume.cc',
]

# Dir-wide AM_CPPFLAGS includes, shared by the norton lib and executables
norton_inc_list = [
  config_h_include,
  base_inc,
  util_inc,
  vicedep_inc,
  dir_inc,
  kerndep_inc,
  al_inc,
  partition_inc,
  auth2_inc,
  vv_inc,
  lka_inc,
  vol_inc,
  resolution_inc,
  volutil_inc,
]

libnorton_lib = static_library('norton',
  norton_sources,
  build_by_default : false,
  include_directories : norton_inc_list,
  dependencies : [rvm_rpc2_dep],
)

norton_inc = include_directories('.')

norton_dep = declare_dependency(
  include_directories : norton_inc,
  link_with : libnorton_lib,
)

if not get_option('build_server').disabled()
  norton_tool_deps = [
    norton_dep, resolution_dep, vol_dep, volutil_dep,
    vv_dep, partition_dep, al_dep, dir_dep, util_dep,
    rwcdb_dep, base_dep, rvm_rpc2_dep,
  ]

  executable('norton',
    ['norton.cc', 'norton.h', 'dummy.cc'],
    include_directories : norton_inc_list,
    dependencies : norton_tool_deps,
    link_with : [libnorton_lib],
    install : true,
    install_dir : get_option('sbindir'),
  )

  executable('norton-reinit',
    ['norton-reinit.cc', 'dummy.cc'],
    include_directories : norton_inc_list,
    dependencies : norton_tool_deps,
    link_with : [libnorton_lib],
    install : true,
    install_dir : get_option('sbindir'),
  )

  install_data('reinit',
    install_dir : get_option('bindir'),
    install_mode : 'rwxr-xr-x',
  )

  install_man('norton.8')
endif
