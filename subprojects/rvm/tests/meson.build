testrvm = executable(
    'testrvm',
    ['testrvm.c', 'testrvm.h'],
    dependencies: [rvm_dep],
)
test(
    'testrvm',
    files('run_testrvm.sh'),
    args: [testrvm],
)

rvm_basher = executable(
    'rvm_basher',
    ['rvm_basher.c'],
    include_directories: [config_h_include],
    dependencies: [rds_dep],
)
test(
    'rvm_basher',
    files('run_basher.sh'),
    args: [rvm_basher, rvmutl, rdsinit],
    timeout: 120,
)

rvmlwp_basher = executable(
    'rvmlwp_basher',
    ['rvm_basher.c'],
    include_directories: [config_h_include],
    c_args: ['-DRVM_USELWP'],
    dependencies: [rdslwp_dep],
)
test(
    'rvmlwp_basher',
    files('run_basher.sh'),
    args: [rvmlwp_basher, rvmutl, rdsinit],
    is_parallel: false,
    timeout: 120,
)

rvmpt_basher = executable(
    'rvmpt_basher',
    ['rvm_basher.c'],
    include_directories: [config_h_include],
    c_args: ['-DRVM_USEPT'],
    dependencies: [rdspt_dep],
)
test(
    'rvmpt_basher',
    files('run_basher.sh'),
    args: [rvmpt_basher, rvmutl, rdsinit],
    is_parallel: false,
    timeout: 120,
)

have_memcheck_h = compiler.has_header('valgrind/memcheck.h')

# Link gtest directly instead of using gtest_dep: gtest_dep propagates the
# threads dependency (-pthread), which defines _REENTRANT and makes lwp.h
# report the pthread LWP_VERSION, mismatching the non-pthread lwp library.
gtest_sub = subproject('gtest', required: false)
if gtest_sub.found() and have_memcheck_h
    gtest_inc = gtest_sub.get_variable('gtest_incdir')
    gtest_lib = gtest_sub.get_variable('gtest_lib')

    rvm_ut = executable(
        'rvm_ut',
        [
            'main.cc',
            'rvm_ut.cc',
        ],
        include_directories: [
            config_h_include,
            include_directories('include'),
            gtest_inc,
        ],
        dependencies: [rvmlwp_dep],
        link_with: [gtest_lib],
    )

    test(
        'rvm_unit',
        rvm_ut,
        suite: 'unit',
        args: [
            '--gtest_shuffle',
            '--gtest_death_test_style=threadsafe',
        ],
        timeout: 400,
    )
endif
