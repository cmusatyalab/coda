kerndep_sources = [
  'coda.h',
  'pioctl.c',
  'pioctl.h',
]

kerndep_lib = static_library('kerndep',
  kerndep_sources,
  build_by_default : false,
  include_directories : [config_h_include],
  dependencies : base_dep,
)

kerndep_inc = include_directories('.')

kerndep_dep = declare_dependency(
  include_directories : kerndep_inc,
  link_with : kerndep_lib,
)
