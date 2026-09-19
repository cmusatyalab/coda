rp2gen_sources = [
    'crout.c',
    'rp2.h',
    'rp2main.c',
    'symtab.c',
    'util.c',
]

bison = find_program('bison')
parser_source = custom_target(
    'rp2gen_parser',
    input: 'rp2gen_parser.y',
    output: ['rp2gen_parser.c', 'rp2gen_parser.h'],
    command: [bison, '--defines=@OUTPUT1@', '-o', '@OUTPUT0@', '@INPUT@'],
)

flex = find_program('flex')
lexer_source = custom_target(
    'rp2gen_lexer',
    input: 'rp2gen_lexer.l',
    output: 'rp2gen_lexer.c',
    command: [flex, '-o', '@OUTPUT0@', '@INPUT@'],
)

rp2gen_dev = executable(
    'rp2gen-dev',
    [rp2gen_sources, parser_source, lexer_source],
    include_directories: [config_h_include, rpc2_inc],
    c_args: '-Wno-unused-function',
    build_by_default: false,
    native: true,
)
meson.override_find_program('rp2gen', rp2gen_dev)

rp2gen = executable(
    'rp2gen',
    [rp2gen_sources, parser_source, lexer_source],
    include_directories: [config_h_include, rpc2_inc],
    c_args: '-Wno-unused-function',
    install: true,
    install_tag: 'devel',
)
