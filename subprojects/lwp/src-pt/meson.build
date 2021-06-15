lwp_pt_sources = [
  'dllist.h',
  'iomgr_pt.c',
  fasttime_c,
  'lock_pt.c',
  'lwp_pt.c',
  'lwp.private_pt.h',
  'preempt_pt.c',
  timer_c,
]

liblwp_pt = library('lwp_pt',
  lwp_pt_sources,
  version : lt_version,
  include_directories : [config_h_include, lwp_inc],
  dependencies : [pthread_dep],
  c_args : '-D_REENTRANT',
  install : true
)

lwp_pt_dep = declare_dependency(
  include_directories : lwp_inc,
  compile_args : '-D_REENTRANT',
  link_with : liblwp_pt,
)


# test are inherited from src/meson.build
foreach t : tests
  test = executable(t[0], t[1], dependencies : [lwp_pt_dep])
  test(t[0], test, args : t[2])
endforeach
