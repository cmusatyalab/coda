rp2gen = rpc2_sub.get_variable('rp2gen')

auth2 = custom_target('auth2_rpc2',
  input : 'auth2.rpc2',
  output : [
    'auth2.h',
    'auth2.client.c',
    'auth2.server.c',
    'auth2.multi.c',
    'auth2.print.c',
    'auth2.helper.c',
  ],
  command : [rp2gen, '-C', '@OUTDIR@', '-I', '@CURRENT_SOURCE_DIR@', '@INPUT@'],
)

rpc2 = {
  'auth2' : {
    'header' : auth2[0],
    'client' : auth2[1],
    'server' : auth2[2],
    'multi' : auth2[3],
    'print' : auth2[4],
    'helper' : auth2[5],
  },
}

auth2_inc = include_directories('.')

auser_sources = [
  'auser.c',
  'auser.h',
  'avenus.c',
  'avenus.h',
]

auth2_sources = [
  'avice.c',
  'avice.h',
  'codatoken.c',
  'codatoken.h',
  'pwsupport.c',
  'pwsupport.h',
]

# libauser contains the auth2 client stubs, from Makefile.am
libauser_lib = static_library('auser',
  auser_sources + [
    rpc2['auth2']['header'],
    rpc2['auth2']['client'],
    rpc2['auth2']['helper'],
  ],
  build_by_default : false,
  include_directories : [config_h_include, base_inc, kerndep_inc, util_inc,
                         al_inc, auth2_inc],
  dependencies : [kerndep_dep, util_dep, base_dep, rpc2_dep],
)

# libauth2 contains the auth2 server stubs, from Makefile.am
libauth2_lib = static_library('auth2',
  auth2_sources + [
    rpc2['auth2']['header'],
    rpc2['auth2']['server'],
    rpc2['auth2']['helper'],
  ],
  build_by_default : false,
  include_directories : [config_h_include, base_inc, kerndep_inc, util_inc,
                         al_inc, auth2_inc],
  dependencies : [al_dep, rwcdb_dep, base_dep, rpc2_dep],
)

if not get_option('build_client').disabled()
  executable('au',
    ['au.c'],
    include_directories : [
      config_h_include, base_inc, kerndep_inc, util_inc,
      al_inc, auth2_inc,
    ],
    dependencies : [kerndep_dep, util_dep, base_dep, rpc2_dep],
    link_with : [libauser_lib],
    install : true,
  )

  executable('clog',
    ['clog.c', 'tokenfile.c', 'tokenfile.h'],
    include_directories : [
      config_h_include, base_inc, kerndep_inc, util_inc,
      al_inc, auth2_inc,
    ],
    dependencies : [kerndep_dep, util_dep, base_dep, rpc2_dep],
    link_with : [libauser_lib],
    install : true,
  )

  executable('cpasswd',
    ['cpasswd.c'],
    include_directories : [
      config_h_include, base_inc, kerndep_inc, util_inc,
      al_inc, auth2_inc,
    ],
    dependencies : [kerndep_dep, util_dep, base_dep, rpc2_dep],
    link_with : [libauser_lib],
    install : true,
  )

  executable('ctokens',
    ['ctokens.c'],
    include_directories : [
      config_h_include, base_inc, kerndep_inc, util_inc,
      al_inc, auth2_inc,
    ],
    dependencies : [kerndep_dep, util_dep, base_dep, rpc2_dep],
    link_with : [libauser_lib],
    install : true,
  )

  executable('cunlog',
    ['cunlog.c'],
    include_directories : [
      config_h_include, base_inc, kerndep_inc, util_inc,
      al_inc, auth2_inc,
    ],
    dependencies : [kerndep_dep, util_dep, base_dep, rpc2_dep],
    link_with : [libauser_lib],
    install : true,
  )

  install_man(['au.1', 'clog.1', 'cpasswd.1', 'ctokens.1', 'cunlog.1'])
endif

if not get_option('build_server').disabled()
  executable('auth2',
    ['auth2.c'],
    include_directories : [
      config_h_include, base_inc, kerndep_inc, util_inc,
      al_inc, auth2_inc,
    ],
    dependencies : [
      al_dep, util_dep, rwcdb_dep,
      base_dep, rpc2_dep,
    ],
    link_with : [libauth2_lib],
    install : true,
    install_dir : get_option('sbindir'),
  )

  executable('initpw',
    ['initpw.c'],
    include_directories : [
      config_h_include, base_inc, kerndep_inc, util_inc,
      al_inc, auth2_inc,
    ],
    dependencies : [base_dep, rpc2_dep],
    install : true,
    install_dir : get_option('sbindir'),
  )

  executable('tokentool',
    ['tokentool.c', 'tokenfile.c', 'tokenfile.h'],
    include_directories : [
      config_h_include, base_inc, kerndep_inc, util_inc,
      al_inc, auth2_inc,
    ],
    dependencies : [al_dep, util_dep, rwcdb_dep, base_dep, rpc2_dep],
    link_with : [libauth2_lib],
    install : true,
    install_dir : get_option('sbindir'),
  )

  install_man(['auth2.8', 'initpw.8', 'passwd.coda.5'])
endif

auser_dep = declare_dependency(
  include_directories : auth2_inc,
  link_with : libauser_lib,
)

auth2_dep = declare_dependency(
  include_directories : auth2_inc,
  link_with : libauth2_lib,
)
