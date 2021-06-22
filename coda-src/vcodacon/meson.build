if get_option('vcodacon').enabled() and not get_option('build_client').disabled()
  fluid = find_program('fluid', required : false)

  if fluid.found()
    vcodacon_fl = custom_target(
      'vcodacon.cc',
      input : 'vcodacon.fl',
      output : ['vcodacon.cc', 'vcodacon.h'],
      command : [fluid, '-c', '@INPUT@', '-o', '@CURRENT_SOURCE_DIR@'],
    )

    executable('vcodacon',
      ['Inet.cc', 'monitor.cc', 'util.cc', 'Inet.h', 'monitor.h', 'util.h'] + vcodacon_fl,
      dependencies : [fltk_dep, base_dep],
      link_with : [base_lib],
      install : true,
    )
  endif
endif
