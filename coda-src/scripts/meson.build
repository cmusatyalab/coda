if not get_option('build_client').disabled()
  configure_file(
    input : 'coda-client-setup.in',
    output : '@BASENAME@',
    configuration : substitutions,
    install : true,
    install_dir : 'sbin',
    install_mode : 'rwxr-xr-x',
  )
  install_man('coda-client-setup.8')
endif

if not get_option('build_server').disabled()
  templated = [
    'bldvldb.sh.in',
    'createvol_rep.in',
    'purgevol_rep.in',
    'startserver.in',
    'vice-setup.in',
    'vice-setup-rvm.in',
    'vice-setup-scm.in',
    'vice-setup-srvdir.in',
    'vice-setup-user.in',
  ]
  foreach t : templated
    configure_file(
      input : t,
      output : '@BASENAME@',
      configuration : substitutions,
      install : true,
      install_dir : 'sbin',
      install_mode : 'rwxr-xr-x',
    )
  endforeach

  scripts = [
    'codastart',
    'coda-server-logrotate',
    'partial-reinit.sh',
    'vice-killvolumes',
  ]
  install_data(scripts,
    install_dir : 'sbin',
    install_mode : 'rwxr-xr-x',
  )

  manpages = [
    'bldvldb.sh.8',
    'coda-client-setup.8',
    'createvol_rep.8',
    'maxgroupid.5',
    'purgevol_rep.8',
    'startserver.8',
    'vice-setup.8',
    'volumelist.5',
  ]
  install_man(manpages)
endif
