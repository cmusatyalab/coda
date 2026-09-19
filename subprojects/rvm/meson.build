project(
    'rvm',
    'c',
    'cpp',
    version: '1.30',
    license: 'LGPL-2.0-only',
    default_options: [
        'warning_level=1',
        'cpp_std=c++17',
    ],
    meson_version: '>= 1.1.0',
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
lt_version = '@0@.@1@.@2@'.format(lt_current - lt_age, lt_age, lt_revision)

conf = configuration_data()
compiler = meson.get_compiler('c')

build_lwp = get_option('enable-lwp')
lwp_dep = dependency(
    'lwp',
    required: build_lwp,
    disabler: true,
    fallback: ['lwp', 'lwp_dep'],
)

build_pthreads = get_option('enable-pthread')
pthread_lib = compiler.find_library(
    'pthread',
    required: build_pthreads,
    disabler: true,
)
pthread_dep = declare_dependency(
    compile_args: [
        '-D_REENTRANT',
        '-D_POSIX_PTHREAD_SEMANTIC',
        '-D_THREAD_SAFE',
        '-pthread',
    ],
    dependencies: pthread_lib,
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

configure_file(output: 'config.h', configuration: conf)
add_project_arguments('-DHAVE_CONFIG_H', language: 'c')
config_h_include = include_directories('.')

# installed binaries/libraries must find the libraries in the (non-standard)
# libdir without relying on the system loader search path; only when requested
# so a normal install into a standard libdir stays clean
if get_option('rpath')
    add_project_link_arguments(
        '-Wl,-rpath,' + (get_option('prefix') / get_option('libdir')),
        language: ['c', 'cpp'],
    )
endif

subdir('include')
subdir('rvm')
subdir('seg')
subdir('rds')

# Install paths for the rvmutl and rdsinit tools, exposed so consumers
# (Coda's coda_config.h) can reference them without hardcoding the
# install location.
sbindir_install = get_option('prefix') / get_option('sbindir')
RVMUTL = sbindir_install / 'rvmutl'
RDSINIT = sbindir_install / 'rdsinit'

pkg = import('pkgconfig')
pkg.generate(
    rds_lib,
    name: 'rvm',
    description: 'Persistent memory storage library',
    variables: {
        'RVMUTL': RVMUTL,
        'RDSINIT': RDSINIT,
    },
    uninstalled_variables: {
        'RVMUTL': rvmutl.full_path(),
        'RDSINIT': rdsinit.full_path(),
    },
    url: 'http://coda.cs.cmu.edu',
)

pkg.generate(
    rdslwp_lib,
    name: 'rvmlwp',
    description: 'Persistent memory storage library (LWP build)',
    variables: {
        'RVMUTL': RVMUTL,
        'RDSINIT': RDSINIT,
    },
    uninstalled_variables: {
        'RVMUTL': rvmutl.full_path(),
        'RDSINIT': rdsinit.full_path(),
    },
    url: 'http://coda.cs.cmu.edu',
)

pkg.generate(
    rdspt_lib,
    name: 'rvmpt',
    description: 'Persistent memory storage library (pthread build)',
    variables: {
        'RVMUTL': RVMUTL,
        'RDSINIT': RDSINIT,
    },
    uninstalled_variables: {
        'RVMUTL': rvmutl.full_path(),
        'RDSINIT': rdsinit.full_path(),
    },
    url: 'http://coda.cs.cmu.edu',
)

# The rvmlwp package provides rds+seg+rvm, matching rvmlwp.pc
rvmlwp_dep = declare_dependency(
    include_directories: [rvm_inc],
    dependencies: [lwp_dep],
    link_with: [rdslwp_lib, seglwp_lib, rvmlwp_lib],
    variables: {
        'RVMUTL': RVMUTL,
        'RDSINIT': RDSINIT,
    },
)
meson.override_dependency('rvmlwp', rvmlwp_dep)

subdir('tests')
