rp2gen = rpc2_sub.get_variable('rp2gen')

update = custom_target('update_rpc2',
  input : 'update.rpc2',
  output : [
    'update.h',
    'update.client.c',
    'update.server.c',
    'update.multi.c',
    'update.print.c',
    'update.helper.c',
  ],
  command : [rp2gen, '-C', '@OUTDIR@', '-I', '@CURRENT_SOURCE_DIR@', '@INPUT@'],
)

rpc2 = {
  'update' : {
    'header' : update[0],
    'client' : update[1],
    'server' : update[2],
    'multi' : update[3],
    'print' : update[4],
    'helper' : update[5],
  },
}

update_inc = include_directories('.')

if not get_option('build_server').disabled()
  update_rpc2_sources = [
    rpc2['update']['header'],
    rpc2['update']['client'],
    rpc2['update']['helper'],
  ]

  executable('updateclnt',
    ['updateclnt.cc'] + update_rpc2_sources,
    include_directories : [update_inc, config_h_include],
    dependencies : [
      viceerror_dep, volutildep_dep, util_dep,
      base_dep, rpc2_dep,
    ],
    install : true,
    install_dir : get_option('sbindir'),
  )

  executable('updatefetch',
    ['updatefetch.cc'] + update_rpc2_sources,
    include_directories : [update_inc, config_h_include],
    dependencies : [
      viceerror_dep, volutildep_dep, util_dep,
      base_dep, rpc2_dep,
    ],
    install : true,
    install_dir : get_option('sbindir'),
  )

  updatesrv_rpc2_sources = [
    rpc2['update']['header'],
    rpc2['update']['server'],
    rpc2['update']['helper'],
  ]

  executable('updatesrv',
    ['updatesrv.cc'] + updatesrv_rpc2_sources,
    include_directories : [update_inc, config_h_include],
    dependencies : [
      viceerror_dep, volutildep_dep, util_dep,
      base_dep, rpc2_dep,
    ],
    install : true,
    install_dir : get_option('sbindir'),
  )

  install_man(['updateclnt.8', 'updatesrv.8'])
endif
