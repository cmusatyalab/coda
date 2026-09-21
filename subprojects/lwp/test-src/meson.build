have_memcheck_h = compiler.has_header('valgrind/memcheck.h')

# Link gtest directly instead of using gtest_dep: gtest_dep propagates the
# threads dependency (-pthread), which defines _REENTRANT and makes lwp.h
# report the pthread LWP_VERSION, mismatching the non-pthread lwp library.
gtest_sub = subproject('gtest', required: false)
if gtest_sub.found() and have_memcheck_h
    gtest_inc = gtest_sub.get_variable('gtest_incdir')
    gtest_lib = gtest_sub.get_variable('gtest_lib')

    lwp_ut = executable(
        'lwp_ut',
        [
            'main.cc',
            'lwp_ut.cc',
        ],
        include_directories: [
            config_h_include,
            include_directories('include'),
            gtest_inc,
        ],
        dependencies: [lwp_dep],
        link_with: [gtest_lib],
    )

    test(
        'lwp_unit',
        lwp_ut,
        suite: 'unit',
        args: [
            '--gtest_shuffle',
            '--gtest_death_test_style=threadsafe',
        ],
        timeout: 400,
    )
endif
