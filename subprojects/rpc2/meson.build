project('rpc2', 'c',
  version : '2.39',
  license : 'LGPL-2.0-only',
  default_options : [
    'warning_level=1',
    'default_library=both',
  ],
  meson_version : '>= 1.11.2',
)

# libtool versioning - this applies to librpc2 and libse
#
# - when any library source has changed, increment lt_revision
# - when interfaces were added/removed/changed, increment lt_current and set
#   lt_revision to 0
# - if any interfaces were added, increment lt_age
# - if any interfaces were removed or changed, set lt_age to 0
#
lt_revision = 0
lt_current = 15
lt_age = 9
lt_version = '@0@.@1@.@2@'.format(lt_current-lt_age, lt_age, lt_revision)

conf = configuration_data()
compiler = meson.get_compiler('c')

lwp_dep = dependency('lwp', fallback : ['lwp', 'lwp_dep'])

# for codatunnel
libuv_dep = dependency('libuv', version : '>= 1.4.0')
gnutls_dep = dependency('gnutls')

# lua for network emulation scripts
build_lua = get_option('with-lua')
lua_lib = dependency('lua5.1', required : false, disabler : build_lua.disabled())
if not lua_lib.found()
  lua_lib = dependency('lua', required : build_lua)
endif
if lua_lib.found()
  libm = compiler.find_library('m')
  lua_dep = [lua_lib, libm]
  conf.set('USE_LUA', 1)
endif

# checks for headers
check_headers = [
  ['sys/stream.h', 'HAVE_SYS_STREAM_H'],
  ['arpa/inet.h', 'HAVE_ARPA_INET_H'],
  ['netdb.h', 'HAVE_NETDB_H'],
]
foreach h : check_headers
  if compiler.has_header(h[0])
    conf.set(h[1], 1)
  endif
endforeach

# checks for types
check_types = [
  ['socklen_t', 'HAVE_SOCKLEN_T'],
  ['struct sockaddr_in6', 'HAVE_STRUCT_SOCKADDR_IN6'],
  ['struct sockaddr_storage', 'HAVE_STRUCT_SOCKADDR_STORAGE'],
]
foreach t : check_types
  if compiler.sizeof(t[0], prefix : '#include <sys/types.h>\n#include <sys/socket.h>\n#include <netinet/in.h>') != -1
    conf.set(t[1], 1)
  endif
endforeach

# checks for functions
check_functions = [
  ['ffs', 'HAVE_FFS'],
  ['iopen', 'HAVE_IOPEN'],
  ['getaddrinfo', 'HAVE_GETADDRINFO'],
  ['gai_strerror', 'HAVE_GAI_STRERROR'],
  ['getipnodebyname', 'HAVE_GETIPNODEBYNAME'],
  ['inet_aton', 'HAVE_INET_ATON'],
  ['inet_ntoa', 'HAVE_INET_NTOA'],
  ['inet_pton', 'HAVE_INET_PTON'],
  ['inet_ntop', 'HAVE_INET_NTOP'],
]
foreach f : check_functions
  if compiler.has_function(f[0])
    conf.set(f[1], 1)
  endif
endforeach

configure_file(output : 'config.h', configuration : conf)
add_project_arguments('-DHAVE_CONFIG_H', language : 'c')
config_h_include = include_directories('.')

subdir('include')
subdir('codatunnel')
subdir('secure')
subdir('rp2gen')
subdir('rpc2-src')
subdir('scripts')

if get_option('install-dev')
  pkg = import('pkgconfig')

  pkg.generate(rpc2_lib,
    description : 'Remote Procedure Call library',
    libraries : libse_lib,
    variables : { 'RP2GEN' : rp2gen.full_path() },
    url : 'http://coda.cs.cmu.edu',
  )
endif

# The rpc2 package provides rpc2+se, matching rpc2.pc
rpc2_dep = declare_dependency(
  include_directories : [config_h_include, rpc2_inc],
  dependencies : [lwp_dep],
  link_with : [rpc2_lib, libse_lib],
)
