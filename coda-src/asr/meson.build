asr_sources = [
  'asr.h',
  'path.c',
  'resolver.cc',
  'wildmat.c',
]

if not get_option('build_client').disabled()
  codegen = import('unstable-codegen')

  yacc = codegen.yacc()
  resolver_parser = yacc.generate('resolver_parser.yy', header : '@BASENAME@.hh')

  lex = codegen.lex()
  resolver_lexer = lex.generate('resolver_lexer.l')

  ruletypes_sources = ['ruletypes.cc', 'ruletypes.h']

  executable('parser',
    asr_sources + ruletypes_sources + [resolver_parser, resolver_lexer],
    include_directories : [config_h_include, vicedep_inc],
    dependencies : [
      util_dep, kerndep_dep, vv_dep,
      base_dep, rpc2_dep,
    ],
    link_with : [util_lib],
    install : true,
  )
endif
