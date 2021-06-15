# fasttime.c and timer.c are also included in liblwp_pt
fasttime_c = files('fasttime.c')
timer_c = files('timer.c')

lwp_sources = [
  fasttime_c,
  'iomgr.c',
  'lock.c',
  'lwp.c',
  'lwp.private.h',
  'lwp_stacktrace.c',
  'lwp_stacktrace.h',
  'lwp_ucontext.c',
  'lwp_ucontext.h',
  'process.S',
  timer_c,
  'valgrind.h',
]

lwp_lib = library('lwp',
  lwp_sources,
  version : lt_version,
  include_directories : [config_h_include, lwp_inc],
  c_args : '-DLWPDEBUG',
  install : true
)

lwp_dep = declare_dependency(
  include_directories : lwp_inc,
  link_with : lwp_lib,
)
meson.override_dependency('lwp', lwp_dep)


tests = [
  ['cswitch', files('cswitch.c'), ['1000000']],
  #['tdb', files('tdb.c'), []],  disabled, waits for input
  ['testlwp', files('testlwp.c'), ['5']],
  ['rw', files('rw.c'), []],
]
foreach t : tests
  test = executable(t[0], t[1], dependencies : [lwp_dep])
  test(t[0], test, args : t[2])
endforeach
