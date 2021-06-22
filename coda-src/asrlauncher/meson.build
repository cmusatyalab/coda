if not get_option('build_client').disabled()
  executable('asrlauncher',
    ['asrlauncher.c'],
    include_directories : [config_h_include],
    install : true,
    install_dir : get_option('sbindir'),
  )
endif
