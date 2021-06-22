# Link gtest directly instead of using gtest_dep: gtest_dep propagates the
# threads dependency (-pthread), which defines _REENTRANT and makes lwp.h
# report the pthread LWP_VERSION, mismatching the non-pthread lwp library.
gtest_sub = subproject('gtest')
gtest_inc = gtest_sub.get_variable('gtest_incdir')
gtest_lib = gtest_sub.get_variable('gtest_lib')

unit_inc = include_directories([
  'include',
  '../../coda-src',
])


unit_sources = [
  'lib/lwp/lwp_ut.cc',
  'lib/rvm/rvm_ut.cc',
  'main.cc',
  'util/u_bitmap.cc',
]

unit_exe = executable('unit',
  unit_sources,
  include_directories : [config_h_include, base_inc, unit_inc, gtest_inc],
  cpp_args : ['-DVENUS', '-DTIMING', '-DVENUSDEBUG'],
  dependencies : [util_dep, base_dep, rvm_dep, rpc2_dep],
  link_with : [gtest_lib],
)

test('unit', unit_exe,
  args : [
    '--gtest_shuffle',
    '--gtest_death_test_style=threadsafe',
  ],
)
