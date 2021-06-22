librepair_sources = [
  'cure.cc',
  'cure.h',
  'path.cc',
  'predicate.cc',
  'predicate.h',
  'repcmds.cc',
  'repcmds.h',
  'repio.cc',
  'repio.h',
  'resolve.cc',
  'resolve.h',
  'rvol.cc',
]

librepio_lib = static_library('repio',
  ['repio.cc'],
  build_by_default : false,
  include_directories : [
    config_h_include,
    base_inc,
    util_inc,
    vicedep_inc,
    kerndep_inc,
    al_inc,
    partition_inc,
    auth2_inc,
    vv_inc,
    vol_inc,
  ],
  dependencies : [rpc2_dep],
)

libclnrepair_lib = static_library('clnrepair',
  ['resolve.cc', 'cure.cc', 'predicate.cc', 'repcmds.cc', 'rvol.cc', 'path.cc'],
  build_by_default : false,
  include_directories : [
    config_h_include,
    base_inc,
    util_inc,
    vicedep_inc,
    kerndep_inc,
    al_inc,
    partition_inc,
    auth2_inc,
    vv_inc,
    vol_inc,
  ],
  dependencies : [rpc2_dep],
)

librepair_inc = include_directories('.')

repio_dep = declare_dependency(
  include_directories : librepair_inc,
  link_with : librepio_lib,
)

clnrepair_dep = declare_dependency(
  include_directories : librepair_inc,
  link_with : libclnrepair_lib,
)
