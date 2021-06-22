vtools_client_sources = [
  'coda_replay.cc',
  'coda_replay.h',
]

# Dir-wide AM_CPPFLAGS includes, shared by the vtools executables
vtools_tool_inc = [
  config_h_include,
  base_inc,
  kerndep_inc,
  util_inc,
  vicedep_inc,
  al_inc,
  partition_inc,
  vv_inc,
  venus_inc,
]

if not get_option('build_client').disabled()
  executable('codacon',
    ['codacon.cc'],
    include_directories : vtools_tool_inc,
    dependencies : [base_dep, rpc2_dep],
    link_with : [base_lib],
    install : true,
  )

  executable('cfs',
    ['cfs.cc'],
    include_directories : vtools_tool_inc,
    dependencies : [vv_dep, kerndep_dep, rpc2_dep],
    link_with : [vv_lib, kerndep_lib],
    install : true,
  )

  executable('cmon',
    ['cmon.cc'],
    include_directories : vtools_tool_inc,
    dependencies : [ncurses_dep, rpc2_dep, base_dep],
    link_with : [venusdep_lib],
    install : true,
  )

  executable('coda_replay',
    vtools_client_sources,
    include_directories : vtools_tool_inc,
    install : true,
  )

  executable('hoard',
    ['hoard.cc'],
    include_directories : vtools_tool_inc,
    dependencies : [util_dep, kerndep_dep, rpc2_dep],
    link_with : [util_lib, kerndep_lib],
    install : true,
  )

  executable('spy',
    ['spy.cc'],
    include_directories : vtools_tool_inc,
    dependencies : [base_dep, rpc2_dep],
    link_with : [base_lib],
    install : true,
  )

  executable('mkcodabf',
    ['mkcodabf.c'],
    include_directories : vtools_tool_inc,
    dependencies : [kerndep_dep, rpc2_dep],
    link_with : [kerndep_lib],
    install : true,
  )

  configure_file(
    input : 'gcodacon.in',
    output : 'gcodacon',
    configuration : substitutions,
    install : true,
    install_dir : get_option('bindir'),
    install_mode : 'rwxr-xr-x',
  )

  install_man(['cfs.1', 'cmon.1', 'coda_replay.1', 'hoard.1', 'mkcodabf.1', 'spy.1'])
endif


executable('codaconfedit',
  ['codaconfedit.c'],
  include_directories : [config_h_include, base_inc],
  dependencies : [base_dep],
  link_with : [base_lib],
  install : true,
  install_dir : get_option('sbindir'),
)
