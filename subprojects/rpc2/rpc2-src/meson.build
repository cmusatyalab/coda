rpc2_sources = [
  'cbuf.c',
  'cbuf.h',
  'compat.c',
  'conn.c',
  'ct.c',
  'debug.c',
  'delay.c',
  'dllist.c',
  'dllist.h',
  'errors.c',
  'ffs.c',
  'globals.c',
  'host.c',
  'lists.c',
  'lua.c',
  'mkcall.c',
  'multi1.c',
  'multi2.c',
  'multi3.c',
  'pack_helper.c',
  'packet.c',
  'rpc2.private.h',
  'rpc2_addrinfo.c',
  'rpc2a.c',
  'rpc2b.c',
  'se.c',
  'secure.c',
  'sl.c',
  'switchc2s.h',
  'switchs2c.h',
  'switchs2e.h',
  'trace.h',
]

rpc2_lib = library('rpc2',
  rpc2_sources,
  version : lt_version,
  include_directories : [config_h_include, rpc2_inc],
  dependencies : [secure_dep, codatunnel_dep, lwp_dep, lua_dep],
  c_args : '-DRPC2DEBUG',
  install : true
)

rpc2_dep = declare_dependency(
  include_directories : [config_h_include, rpc2_inc],
  dependencies : [lwp_dep],
  link_with : rpc2_lib,
)

meson.override_dependency('rpc2', rpc2_dep)

libse_sources = [
  'ftp_proto.c',
  'sftp1.c',
  'sftp2.c',
  'sftp3.c',
  'sftp4.c',
  'sftp5.c',
  'sftp6.c',
  'tcpftp1.c',
]

libse_lib = library('se',
  libse_sources,
  version : lt_version,
  include_directories : [config_h_include],
  dependencies : [rpc2_dep, codatunnel_dep, lwp_dep],
  c_args : '-DRPC2DEBUG',
  install : true
)

libse_dep = declare_dependency(
  include_directories : [config_h_include, rpc2_inc],
  link_with : libse_lib,
)

meson.override_dependency('libse', libse_dep)


tests = [
  ['ctest', 'ctest.c'],
  ['stest', 'stest.c'],
  ['rpc2test', 'rpc2test.c'],
]
foreach t : tests
  test = executable(t[0], t[1], dependencies : [rpc2_dep, libse_dep])
  # don't automatically run these tests, they require user interaction
endforeach
