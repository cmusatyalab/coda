al_sources = [
  'alprocs.c',
  'pdb.c',
  'pdbarray.c',
  'pdbdb.c',
  'pdbpack.c',
  'pdbprofile.c',
]

al_lib = static_library('al',
  al_sources,
  build_by_default : false,
  include_directories : [config_h_include, base_inc, util_inc],
  dependencies : [rwcdb_dep],
)

al_inc = include_directories('.')

al_dep = declare_dependency(
  include_directories : al_inc,
  link_with : al_lib,
)

if not get_option('build_server').disabled()
  pdbtool_sources = ['pdbtool.c']
  executable('pdbtool',
    pdbtool_sources,
    include_directories : [config_h_include, base_inc, util_inc],
    dependencies : [util_dep, rwcdb_dep, base_dep],
    link_with : [al_lib],
    install : true,
    install_dir : get_option('sbindir'),
  )

  install_man('pdbtool.8')
endif
