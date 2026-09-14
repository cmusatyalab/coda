project('lwp', 'c',
  version : '2.19',
  license : 'LGPL-2.0-only',
  default_options : [
    'warning_level=1',
    'default_library=both',
  ],
  meson_version : '>= 1.11.2',
)

# libtool versioning - this applies to liblwp
#
# - when any library source has changed, increment lt_revision
# - when interfaces were added/removed/changed, increment lt_current and set
#   lt_revision to 0
# - if any interfaces were added, increment lt_age
# - if any interfaces were removed or changed, set lt_age to 0
#
lt_revision = 3
lt_current = 3
lt_age = 1
lt_version = '@0@.@1@.@2@'.format(lt_current-lt_age, lt_age, lt_revision)

compiler = meson.get_compiler('c')

build_pthread = get_option('enable-pthread')
pthread_dep = compiler.find_library('pthread', required : build_pthread, disabler : true)
if pthread_dep.found()
  posix4_dep = compiler.find_library('posix4', required : false)
  pthread_dep = [ pthread_dep, posix4_dep ]
endif

conf = configuration_data()

conf.set10('HAVE_REGISTER_T', compiler.sizeof('register_t', prefix : '#include <sys/types.h>') != -1)
conf.set10('HAVE_STACK_T', compiler.sizeof('stack_t', prefix : '#include <signal.h>') != -1)

conf.set10('HAVE_SIGALTSTACK', compiler.has_function('sigaltstack'))
conf.set10('HAVE_MMAP', compiler.has_function('mmap'))

configure_file(output : 'config.h', configuration : conf)
add_project_arguments('-DHAVE_CONFIG_H', language : 'c')
config_h_include = include_directories('.')

subdir('include')
subdir('src')
subdir('src-pt')

if get_option('install-dev')
  pkg = import('pkgconfig')

  pkg.generate(lwp_lib,
    description : 'Light Weight Processes, userspace thread library',
    url : 'http://coda.cs.cmu.edu',
  )

  pkg.generate(liblwp_pt,
    description : 'Light Weight Processes, userspace thread library (pthread build)',
    url : 'http://coda.cs.cmu.edu',
  )
endif
