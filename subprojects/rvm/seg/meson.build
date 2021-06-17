seg_sources = [
  'rvm_createseg.c',
  'rvm_loadseg.c',
  'rvm_releaseseg.c',
  'rvm_segment_private.h',
  'rvm_segutil.c',
]

seg_lib = library('seg',
  seg_sources,
  version : lt_version,
  include_directories : [config_h_include, rvm_inc],
  dependencies : [rvm_dep],
  install : true,
)

seg_dep = declare_dependency(
  include_directories : [rvm_inc],
  dependencies : [rvm_dep],
  link_with : seg_lib,
)

seglwp_lib = library('seglwp',
  seg_sources,
  version : lt_version,
  include_directories : [config_h_include, rvm_inc],
  c_args : ['-DRVM_USELWP'],
  dependencies : [rvmlwp_dep],
  install : true,
)

seglwp_dep = declare_dependency(
  include_directories : [rvm_inc],
  dependencies : [rvmlwp_dep],
  link_with : seglwp_lib,
)

segpt_lib = library('segpt',
  seg_sources,
  version : lt_version,
  include_directories : [config_h_include, rvm_inc],
  c_args : ['-DRVM_USEPT'],
  dependencies : [rvmpt_dep],
  install : true,
)

segpt_dep = declare_dependency(
  include_directories : [rvm_inc],
  dependencies : [rvmpt_dep],
  link_with : segpt_lib,
)
