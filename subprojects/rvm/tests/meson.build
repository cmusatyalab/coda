testrvm = executable('testrvm',
  ['testrvm.c', 'testrvm.h'],
  dependencies : [rvm_dep],
)
test('testrvm', files('run_testrvm.sh'),
  args : [testrvm],
)

rvm_basher = executable('rvm_basher',
  ['rvm_basher.c'],
  include_directories : [config_h_include],
  dependencies : [rds_dep],
)
test('rvm_basher', files('run_basher.sh'),
  args : [rvm_basher, rvmutl, rdsinit],
)

rvmlwp_basher = executable('rvmlwp_basher',
  ['rvm_basher.c'],
  include_directories : [config_h_include],
  c_args : ['-DRVM_USELWP'],
  dependencies : [rdslwp_dep],
)
test('rvmlwp_basher', files('run_basher.sh'),
  args : [rvmlwp_basher, rvmutl, rdsinit],
  is_parallel : false,
)

rvmpt_basher = executable('rvmpt_basher',
  ['rvm_basher.c'],
  include_directories : [config_h_include],
  c_args : ['-DRVM_USEPT'],
  dependencies : [rdspt_dep],
)
test('rvmpt_basher', files('run_basher.sh'),
  args : [rvmpt_basher, rvmutl, rdsinit],
  is_parallel : false,
)
