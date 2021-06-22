lka_sources = [
  'lka.c',
  'lka.h',
  'lka_private.h',
  'shaprocs.c',
]

lka_lib = static_library('lka',
  lka_sources,
  build_by_default : false,
  include_directories : [config_h_include, base_inc],
  dependencies : [rwcdb_dep, lwp_dep],
)

lka_inc = include_directories('.')

lka_dep = declare_dependency(
  include_directories : lka_inc,
  link_with : lka_lib,
)

if not get_option('build_client').disabled()
  executable('mklka',
    ['mklka.c'],
    include_directories : [config_h_include],
    dependencies : [base_dep, rwcdb_dep],
    link_with : [lka_lib],
    install : true,
  )
endif
