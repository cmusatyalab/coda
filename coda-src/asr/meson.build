asr_sources = [
    'asr.h',
    'path.c',
    'resolver.cc',
    'wildmat.c',
]

if not get_option('build_client').disabled()
    bison = find_program('bison')
    resolver_parser = custom_target(
        'resolver_parser',
        input: 'resolver_parser.yy',
        output: ['resolver_parser.cpp', 'resolver_parser.hh'],
        command: [bison, '--defines=@OUTPUT1@', '-o', '@OUTPUT0@', '@INPUT@'],
    )

    flex = find_program('flex')
    resolver_lexer = custom_target(
        'resolver_lexer',
        input: 'resolver_lexer.l',
        output: 'resolver_lexer.c',
        command: [flex, '-o', '@OUTPUT0@', '@INPUT@'],
    )

    ruletypes_sources = ['ruletypes.cc', 'ruletypes.h']

    executable(
        'parser',
        asr_sources + ruletypes_sources + [resolver_parser, resolver_lexer],
        include_directories: [config_h_include],
        dependencies: [
            util_dep,
            kerndep_dep,
            vv_dep,
            base_dep,
            rpc2_dep,
            vicedep_gen_dep,
        ],
        link_with: [util_lib],
        install: true,
    )
endif
