codatunnel_sources = [
  'codatunnel.c',
  'codatunnel.private.h',
  'codatunneld.c',
  'codatunneld.private.h',
  'cookie.c',
  'cookie.h',
  'ctp.h',
  'remotedest.c',
]

codatunnel_inc = include_directories('.')

codatunnel_lib = static_library('codatunnel',
  codatunnel_sources,
  include_directories : [config_h_include, rpc2_inc, codatunnel_inc],
  dependencies : [
    libuv_dep,
    gnutls_dep,
    lwp_dep,
  ]
)

codatunnel_dep = declare_dependency(
  include_directories : [rpc2_inc, codatunnel_inc],
  link_with : codatunnel_lib,
)
