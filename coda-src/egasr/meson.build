# Dir-wide AM_CPPFLAGS includes, shared by the egasr executables
egasr_tool_inc = [
  config_h_include,
  base_inc,
  kerndep_inc,
  vicedep_inc,
  dir_inc,
  vv_inc,
]

if not get_option('build_client').disabled()
  executable('filerepair',
    ['filerepair.c'],
    include_directories : egasr_tool_inc,
    dependencies : [kerndep_dep, base_dep, rvm_rpc2_dep],
    install : true,
  )

  executable('removeinc',
    ['removeinc.c'],
    include_directories : egasr_tool_inc,
    dependencies : [dir_dep, kerndep_dep, base_dep, rvm_rpc2_dep],
    link_with : [dir_lib],
    install : true,
  )

  install_data(['xfrepair', 'xaskuser'],
    install_dir : get_option('bindir'),
    install_mode : 'rwxr-xr-x',
  )
endif
