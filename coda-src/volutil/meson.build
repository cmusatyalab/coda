dumpstuff_sources = [
  'dump.h',
  'dumpstream.cc',
  'dumpstream.h',
  'dumpstuff.cc',
]

libdumpstuff_lib = static_library('dumpstuff',
  dumpstuff_sources,
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
    resolution_inc,
  ],
  dependencies : [rvm_rpc2_dep],
)

volutil_sources = [
  'readstuff.cc',
  'vol-ancient.cc',
  'vol-backup.cc',
  'vol-clone.cc',
  'vol-create.cc',
  'vol-dump.cc',
  'vol-dumpvrdb.cc',
  'vol-getvolumelist.cc',
  'vol-info.cc',
  'vol-lock.cc',
  'vol-lookup.cc',
  'vol-makevldb.cc',
  'vol-makevrdb.cc',
  'vol-maxid.cc',
  'vol-printstats.cc',
  'vol-purge.cc',
  'vol-restore.cc',
  'vol-rvmsize.cc',
  'vol-rvmtrunc.cc',
  'vol-salvage.cc',
  'vol-salvage.private.h',
  'vol-setlogparms.cc',
  'vol-setvv.cc',
  'vol-showcallbacks.cc',
  'vol-showvnode.cc',
  'vol-timing.cc',
  'vol-tracerpc.cc',
  'volutil.private.h',
  'vvlist.cc',
  'vvlist.h',
]

libvolutil_lib = static_library('volutil',
  volutil_sources + dumpstuff_sources,
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
    resolution_inc,
  ],
  dependencies : [rvm_rpc2_dep],
)

libvolserv_lib = static_library('volserv',
  ['volutil.cc'],
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
    resolution_inc,
  ],
  dependencies : [rvm_rpc2_dep],
)

volutil_inc = include_directories('.')

volutil_dep = declare_dependency(
  include_directories : volutil_inc,
  link_with : libvolutil_lib,
)

volserv_dep = declare_dependency(
  include_directories : volutil_inc,
  link_with : libvolserv_lib,
)

# Dir-wide AM_CPPFLAGS includes, shared by the volutil tools
volutil_tool_inc = [
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
]

if not get_option('build_server').disabled()
  executable('volutil',
    ['volclient.cc'],
    include_directories : volutil_tool_inc,
    dependencies : [
      auser_dep, volutildep_dep, util_dep,
      kerndep_dep, rvm_rpc2_dep, libse_dep,
    ],
    link_with : [libvolutil_lib, libvolserv_lib],
    install : true,
    install_dir : get_option('sbindir'),
  )

  codareaddump = executable('codareaddump',
    ['codareaddump.cc'],
    include_directories : volutil_tool_inc,
    dependencies : [vv_dep, vicedep_dep, util_dep, base_dep, rvm_rpc2_dep],
    link_with : [libdumpstuff_lib],
    install : true,
    install_dir : get_option('sbindir'),
  )

  codamergedump = executable('codamergedump',
    ['codamergedump.cc'],
    include_directories : volutil_tool_inc,
    dependencies : [vicedep_dep, util_dep, base_dep, rvm_rpc2_dep],
    link_with : [libdumpstuff_lib],
    install : true,
    install_dir : get_option('sbindir'),
  )

  codadump2tar = executable('codadump2tar',
    ['codadump2tar.cc', 'strencode.c'],
    include_directories : volutil_tool_inc,
    dependencies : [
      al_dep, vol_dep, vv_dep, dir_dep,
      vicedep_dep, util_dep, base_dep, rwcdb_dep, rvm_rpc2_dep
    ],
    link_with : [libdumpstuff_lib],
    install : true,
    install_dir : get_option('sbindir'),
  )

  install_man(['codadumpfile.5', 'codamergedump.8', 'codareaddump.8', 'volutil.8'])
endif
