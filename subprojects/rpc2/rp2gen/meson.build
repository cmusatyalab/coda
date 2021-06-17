rp2gen_sources = [
  'crout.c',
  'rp2.h',
  'rp2main.c',
  'symtab.c',
  'util.c',
]

codegen = import('unstable-codegen')

yacc = codegen.yacc()
parser_source = yacc.generate('rp2gen_parser.y')

lex = codegen.lex()
lexer_source = lex.generate('rp2gen_lexer.l')

rp2gen = executable('rp2gen',
  [rp2gen_sources, parser_source, lexer_source],
  include_directories : [config_h_include, rpc2_inc],
  c_args : '-Wno-unused-function',
  native : true,
  install : get_option('install-dev'),
)
