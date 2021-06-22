project('coda', ['c', 'cpp'],
  version : '8.1.7~dev0',
  license : 'GPL-2.0-only',
  default_options : [
    'warning_level=1',
    'default_library=both',
  ],
  meson_version : '>= 1.11.2',
)

meson.add_dist_script('tools/distbuild.sh')

conf = configuration_data()
compiler = meson.get_compiler('c')

prefix = get_option('prefix')
bindir = prefix / get_option('bindir')
sbindir = prefix / get_option('sbindir')

# lwp, rpc2 and rvm are always built from the in-tree subprojects, never
# from an installed/out-of-tree copy, so the build is reproducible.
lwp_sub = subproject('lwp', default_options : {
  'default_library': 'shared',
  'install-dev' : false,
})
lwp_dep = lwp_sub.get_variable('lwp_dep')

# rp2gen is a build-time code generator used in-tree, never installed
rpc2_sub = subproject('rpc2', default_options : {
  'default_library': 'shared',
  'install-dev' : false,
})
rpc2_dep = rpc2_sub.get_variable('rpc2_dep')
libse_dep = rpc2_sub.get_variable('libse_dep')
rp2gen = rpc2_sub.get_variable('rp2gen')

rvm_sub = subproject('rvm', default_options : {
  'default_library': 'shared',
  'install-dev': false,
  'install-tools' : not get_option('build_server').disabled(),
})
rvm_dep = rvm_sub.get_variable('rvmlwp_dep')
# runtime tool paths for post-install server RVM initialization,
# substituted into coda_config.h
rvmutl = rvm_sub.get_variable('rvmutl_path')
rdsinit = rvm_sub.get_variable('rdsinit_path')

rvm_rpc2_dep = [rvm_dep, rpc2_dep]

kvm_dep = compiler.find_library('kvm', required : false)
nsl_dep = compiler.find_library('nsl', required : false)
ncurses_dep = compiler.find_library('ncurses', required : false)
readline_dep = compiler.find_library('readline', required : false)
resolv_dep = compiler.find_library('resolv', required : false)
socket_dep = compiler.find_library('socket', required : false)

fltk_dep = dependency('fltk', required : false)

# checks for headers
check_headers = [
  ['arpa/inet.h', 'HAVE_ARPA_INET_H'],
  ['arpa/nameser.h', 'HAVE_ARPA_NAMESER_H'],
  ['execinfo.h', 'HAVE_EXECINFO_H'],
  ['fts.h', 'HAVE_FTS_H'],
  ['ncurses.h', 'HAVE_NCURSES_H'],
  ['ncurses/ncurses.h', 'HAVE_NCURSES_NCURSES_H'],
  ['netdb.h', 'HAVE_NETDB_H'],
  ['netinet/in.h', 'HAVE_NETINET_IN_H'],
  ['osreldate.h', 'HAVE_OSRELDATE_H'],
  ['resolv.h', 'HAVE_RESOLV_H'],
  ['search.h', 'HAVE_SEARCH_H'],
  ['sys/endian.h', 'HAVE_SYS_ENDIAN_H'],
  ['sys/ioccom.h', 'HAVE_SYS_IOCCOM_H'],
  ['sys/mount.h', 'HAVE_SYS_MOUNT_H'],
  ['sys/param.h', 'HAVE_SYS_PARAM_H'],
  ['sys/random.h', 'HAVE_SYS_RANDOM_H'],
  ['sys/resource.h', 'HAVE_SYS_RESOURCE_H'],
  ['sys/select.h', 'HAVE_SYS_SELECT_H'],
  ['sys/socket.h', 'HAVE_SYS_SOCKET_H'],
  ['sys/stream.h', 'HAVE_SYS_STREAM_H'],
  ['sys/statvfs.h', 'HAVE_SYS_STATVFS_H'],
  ['sys/time.h', 'HAVE_SYS_TIME_H'],
  ['sys/types.h', 'HAVE_SYS_TYPES_H'],
  ['sys/un.h', 'HAVE_SYS_UN_H'],
  ['sys/vfs.h', 'HAVE_SYS_VFS_H'],
  ['ucred.h', 'HAVE_UCRED_H'],
]
foreach h : check_headers
  if compiler.has_header(h[0])
    conf.set(h[1], 1)
  endif
endforeach

# checks for types
check_types = [
  ['int32_t', 'HAVE_INT32_T'],
  ['uint8_t', 'HAVE_UINT8_T'],
  ['uint16_t', 'HAVE_UINT16_T'],
  ['uint32_t', 'HAVE_UINT32_T'],
  ['socklen_t', 'HAVE_SOCKLEN_T'],
  ['struct in6_addr', 'HAVE_STRUCT_IN6_ADDR'],
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
  ['backtrace', [], 'HAVE_BACKTRACE'],
  ['bind', [socket_dep], 'HAVE_BIND'],
  ['clock_gettime', [], 'HAVE_CLOCK_GETTIME'],
  ['fseeko', [], 'HAVE_FSEEKO'],
  ['gethostbyname', [resolv_dep], 'HAVE_GETHOSTBYNAME'],
  ['getpeereid', [], 'HAVE_GETPEEREID'],
  ['getpeerucred', [], 'HAVE_GETPEERUCRED'],
  ['getrandom', [], 'HAVE_GETRANDOM'],
  ['inet_aton', [nsl_dep], 'HAVE_INET_ATON'],
  ['inet_ntoa', [nsl_dep], 'HAVE_INET_NTOA'],
  ['kvm_openfiles', [kvm_dep], 'HAVE_KVM_OPENFILES'],
  ['mmap', [], 'HAVE_MMAP'],
  ['nmount', [], 'HAVE_NMOUNT'],
  ['res_search', [resolv_dep], 'HAVE_RES_SEARCH'],
  ['__res_search', [resolv_dep], 'HAVE___RES_SEARCH'],
  ['res_9_init', [resolv_dep], 'HAVE_RES_9_INIT'],
  ['rl_completion_matches', [readline_dep], 'HAVE_RL_COMPLETION_MATCHES'],
  ['select', [], 'HAVE_SELECT'],
  ['setenv', [], 'HAVE_SETENV'],
  ['snprintf', [], 'HAVE_SNPRINTF'],
  ['statfs', [], 'HAVE_STATFS'],
  ['strerror', [], 'HAVE_STRERROR'],
  ['strtol', [], 'HAVE_STRTOL'],
]
foreach f : check_functions
  if compiler.has_function(f[0], dependencies : f[1])
    conf.set(f[2], 1)
  endif
endforeach

have_fts = compiler.has_function('fts_open', prefix : '#include <fts.h>')

# ...rwcdb checks...
rwcdb_check_headers = [
  ['byteswap.h', 'HAVE_BYTESWAP_H'],
  ['sys/bswap.h', 'HAVE_SYS_BSWAP_H'],
  ['sys/endian.h', 'HAVE_SYS_ENDIAN_H'],
]
foreach h : rwcdb_check_headers
  if compiler.has_header(h[0])
    conf.set(h[1], 1)
  endif
endforeach

# checks for functions
if compiler.has_function('pread', prefix : '''
#define __USE_UNIX98
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 500
#include <unistd.h>''')
  conf.set('HAVE_PREAD', 1)
endif

if meson.get_compiler('c').get_id() == 'clang'
  add_project_arguments('-Wthread-safety', language : ['c', 'cpp'])
endif
add_project_arguments('-fno-exceptions', language : ['c', 'cpp'])
add_project_arguments('-fcheck-new', language : 'cpp')

conf.set('PACKAGE_NAME', '"' + meson.project_name() + '"')
conf.set('PACKAGE_VERSION', '"' + meson.project_version() + '"')

configure_file(output : 'config.h', configuration : conf)
add_project_arguments('-DHAVE_CONFIG_H', language : ['c', 'cpp'])
config_h_include = include_directories('.')

# cputype and systype, mirroring configs/codaconf.m4
cputype = host_machine.cpu()
if cputype.startswith('i') and cputype.endswith('6')
  cputype = 'i386'
endif
shortsys = host_machine.system()
if shortsys.startswith('netbsd')
  shortsys = 'nbsd'
elif shortsys.startswith('freebsd')
  shortsys = 'fbsd_elf'
elif shortsys.startswith('linux')
  shortsys = 'linux'
elif shortsys.startswith('solaris2')
  shortsys = 'solaris2'
endif
systype = cputype + '_' + shortsys

# sysconfdirx, mirroring configs/codaconf.m4
if prefix == '/usr'
  sysconfdirx = '/etc/coda'
else
  sysconfdirx = prefix / 'etc' / 'coda'
  foreach part : prefix.split('/')
    if part.startswith('coda')
      sysconfdirx = prefix / 'etc'
    endif
  endforeach
endif

substitutions = configuration_data({
  'prefix' : prefix,
  'exec_prefix' : prefix,
  'bindir' : bindir,
  'sbindir' : sbindir,
  'sysconfdir' : get_option('sysconfdir'),
  'sysconfdirx' : sysconfdirx,
  'cputype' : cputype,
  'systype' : systype,
  'PYTHON' : '/usr/bin/env python3',
  'RVMUTL' : rvmutl,
  'RDSINIT' : rdsinit,
})

configure_file(
  input : 'coda_config.h.in',
  output : 'coda_config.h',
  configuration : substitutions,
)

# Coda configuration files (mirrors autotools dist_sysconf_DATA and
# dist_certificate_DATA), installed to sysconfdirx.
install_data('tools/Coda_CA.crt', install_dir : sysconfdirx / 'ssl')

subdir('lib-src')
subdir('coda-src')
subdir('test-src')

# for building documentation, if found adds the 'docs' compile target
rsync = find_program('rsync', disabler : true, required : false)
uvx = find_program('uvx', disabler : true, required : false)
if rsync.found() and uvx.found()
  run_target('docs', command : ['tools/build-docs.sh'])
endif
