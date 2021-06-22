smon_sources = [
  'getvolinfo.c',
  'rpc2ping.c',
  'rvmsizer.c',
  'smon2.c',
]

if not get_option('build_server').disabled()
  executable('getvolinfo',
    ['getvolinfo.c'],
    include_directories : [config_h_include, vicedep_inc],
    dependencies : [base_dep, rpc2_dep],
    link_with : [venusdep_lib],
    install : true,
  )

  executable('rpc2ping',
    ['rpc2ping.c'],
    include_directories : [config_h_include, vicedep_inc],
    dependencies : [base_dep, rpc2_dep],
    link_with : [venusdep_lib],
    install : true,
  )

  executable('rvmsizer',
    ['rvmsizer.c'],
    include_directories : [config_h_include, vicedep_inc],
    dependencies : [base_dep, rpc2_dep],
    install : true,
  )

  executable('smon2',
    ['smon2.c'],
    include_directories : [config_h_include, vicedep_inc],
    dependencies : [base_dep, rpc2_dep],
    link_with : [venusdep_lib],
    install : true,
  )
endif
