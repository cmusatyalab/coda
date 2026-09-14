project('rvm', 'c',
  version : '1.29',
  license : 'LGPL-2.0-only',
  default_options : [
    'warning_level=1',
    'default_library=both',
  ],
  meson_version : '>= 1.11.2',
)

# libtool versioning - this applies to librds, librvm and libseg
#
# - when any library source has changed, increment lt_revision
# - when interfaces were added/removed/changed, increment lt_current and set
#   lt_revision to 0
# - if any interfaces were added, increment lt_age
# - if any interfaces were removed or changed, set lt_age to 0
#
lt_revision = 9
lt_current = 3
lt_age = 2
lt_version = '@0@.@1@.@2@'.format(lt_current-lt_age, lt_age, lt_revision)

conf = configuration_data()
compiler = meson.get_compiler('c')

build_lwp = get_option('enable-lwp')
lwp_dep = dependency('lwp', required : build_lwp, disabler : true, fallback : ['lwp', 'lwp_dep'])

build_pthreads = get_option('enable-pthread')
pthread_lib = compiler.find_library('pthread', required : build_pthreads, disabler : true)
pthread_dep = declare_dependency(
  compile_args : ['-D_REENTRANT', '-D_POSIX_PTHREAD_SEMANTIC', '-D_THREAD_SAFE', '-pthread'],
  dependencies : pthread_lib,
)

# checks for functions
check_functions = [
  ['fdatasync', 'HAVE_FDATASYNC'],
  ['getpagesize', 'HAVE_GETPAGESIZE'],
  ['mmap', 'HAVE_MMAP'],
  ['strerror', 'HAVE_STRERROR'],
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
subdir('rvm')
subdir('seg')
subdir('rds')
subdir('tests')

if get_option('install-dev')
  pkg = import('pkgconfig')

  pkg.generate(rds_lib,
    name : 'rvm',
    description : 'Persistent memory storage library',
    variables : {
      'RVMUTL' : rvmutl.full_path(),
      'RDSINIT' : rdsinit.full_path(),
    },
    url : 'http://coda.cs.cmu.edu',
  )

  pkg.generate(rdslwp_lib,
    name : 'rvmlwp',
    description : 'Persistent memory storage library (LWP build)',
    variables : {
      'RVMUTL' : rvmutl.full_path(),
      'RDSINIT' : rdsinit.full_path(),
    },
    url : 'http://coda.cs.cmu.edu',
  )

  pkg.generate(rdspt_lib,
    name : 'rvmpt',
    description : 'Persistent memory storage library (pthread build)',
    variables : {
      'RVMUTL' : rvmutl.full_path(),
      'RDSINIT' : rdsinit.full_path(),
    },
    url : 'http://coda.cs.cmu.edu',
  )
endif

# The rvmlwp package provides rds+seg+rvm, matching rvmlwp.pc
rvmlwp_dep = declare_dependency(
  include_directories : [rvm_inc],
  dependencies : [lwp_dep],
  link_with : [rdslwp_lib, seglwp_lib, rvmlwp_lib],
)
meson.override_dependency('rvmlwp', rvmlwp_dep)

# Install paths for the rvmutl and rdsinit tools, exposed so consumers
# (Coda's coda_config.h) can reference them without hardcoding the
# install location.
sbindir_install = get_option('prefix') / get_option('sbindir')
rvmutl_path  = sbindir_install / 'rvmutl'
rdsinit_path = sbindir_install / 'rdsinit'
