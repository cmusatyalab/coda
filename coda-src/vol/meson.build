vol_sources = [
  'camprivate.h',
  'coda_globals.h',
  'cvnode.cc',
  'cvnode.h',
  'dirvnode.cc',
  'dumpcamstorage.cc',
  'fssync.cc',
  'fssync.h',
  'index.cc',
  'index.h',
  'lockqueue.cc',
  'lockqueue.h',
  'recov.h',
  'recova.cc',
  'recovb.cc',
  'recovc.cc',
  'struct.h',
  'treeremove.h',
  'vicelock.h',
  'vldb.cc',
  'vldb.h',
  'vlist.cc',
  'vlist.h',
  'vol-debug.cc',
  'voldefs.h',
  'volhash.cc',
  'volhash.h',
  'vollocate.cc',
  'vollocate.h',
  'volres.h',
  'volume.cc',
  'volume.h',
  'vrdb.cc',
  'vrdb.h',
  'vutil.cc',
  'vutil.h',
]

vol_lib = static_library('vol',
  vol_sources,
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
  ],
  dependencies : [rvm_rpc2_dep],
)

vol_inc = include_directories('.')

vol_dep = declare_dependency(
  include_directories : vol_inc,
  link_with : vol_lib,
)

if not get_option('build_server').disabled()
  install_man('vrdb.5')
endif
