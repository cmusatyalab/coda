if not get_option('build_client').disabled()
  executable('repair',
    ['repair.cc', 'repair.h'],
    include_directories : [config_h_include],
    dependencies : [
      vv_dep, auser_dep, partition_dep,
      vicedep_dep, util_dep, base_dep, clnrepair_dep, repio_dep,
      kerndep_dep, rpc2_dep
    ],
    link_with : [libclnrepair_lib, librepio_lib],
    install : true,
  )

  install_man('repair.1')
endif
