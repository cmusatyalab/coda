# Link gtest directly instead of using gtest_dep: gtest_dep propagates the
# threads dependency (-pthread), which defines _REENTRANT and makes lwp.h
# report the pthread LWP_VERSION, mismatching the non-pthread lwp library.
gtest_sub = subproject('gtest', required: false)
if gtest_sub.found()
    gtest_inc = gtest_sub.get_variable('gtest_incdir')
    gtest_lib = gtest_sub.get_variable('gtest_lib')

    rpc2_ut = executable(
        'rpc2_ut',
        [
            'main.cc',
            'cookie_ut.cc',
            'ctp_ut.cc',
            'fileapi_ut.cc',
            'tcpftp_ut.cc',
        ],
        include_directories: [
            codatunnel_inc,
            gtest_inc,
        ],
        dependencies: [rpc2_dep],
        link_with: [gtest_lib],
    )

    test(
        'rpc2_unit',
        rpc2_ut,
        suite: 'unit',
        args: [
            '--gtest_shuffle',
            '--gtest_death_test_style=threadsafe',
        ],
        timeout: 400,
    )
endif
