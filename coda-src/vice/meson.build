vice_sources = [
  'ViceErrorMsg.c',
]

libviceerror_lib = static_library('viceerror',
  vice_sources,
  build_by_default : false,
  include_directories : [
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
    librepair_inc,
    resolution_inc,
  ],
  dependencies : [rvm_rpc2_dep],
)

if not get_option('build_server').disabled()
  codasrv_sources = [
    'clientproc.cc',
    'codaproc.cc',
    'codaproc.h',
    'codaproc2.cc',
    'coppend.cc',
    'coppend.h',
    'smon.cc',
    'srv.cc',
    'srvproc.cc',
    'srvproc2.cc',
    'timecalls.h',
    'vice.private.h',
    'vicecb.cc',
]

  executable('codasrv',
    codasrv_sources,
    include_directories : [config_h_include],
    dependencies : [
      resolution_dep, repio_dep, vol_dep,
      partition_dep, dir_dep, al_dep, auth2_dep,
      vv_dep, lka_dep, vicedep_dep, util_dep,
      base_dep, rwcdb_dep, kerndep_dep, rvm_rpc2_dep
    ],
    link_with : [
      libviceerror_lib,
      libvolutil_lib,
      libvolserv_lib,
      resolution_lib,
      librepio_lib,
      vol_lib,
      partition_lib,
      dir_lib,
      al_lib,
      libauth2_lib,
      vv_lib,
      lka_lib,
      vicedep_lib,
      util_lib,
      rwcdb_lib,
      base_lib,
    ],
    install : true,
    install_dir : get_option('sbindir'),
  )

  executable('printvrdb',
    ['printvrdb.cc'],
    include_directories : [config_h_include, vicedep_inc],
    dependencies : [util_dep, base_dep, rvm_rpc2_dep],
    link_with : [util_lib],
    install : true,
    install_dir : get_option('sbindir'),
  )

  install_man(['codasrv.8', 'servers.5'])

  install_data('server.conf.ex', install_dir : sysconfdirx)
endif

viceerror_inc = include_directories('.')
viceerror_dep = declare_dependency(
  include_directories : viceerror_inc,
  link_with : libviceerror_lib,
)

viceinc_dep = declare_dependency(
  include_directories : viceerror_inc,
  link_with : libviceerror_lib,
)
