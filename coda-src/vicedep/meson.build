rp2gen = rpc2_sub.get_variable('rp2gen')

rpc2_files = [
  'callback.rpc2',
  'cml.rpc2',
  'mond.rpc2',
  'res.rpc2',
  'vice.rpc2',
  'voldump.rpc2',
  'volutil.rpc2',
]

# Process each .rpc2 file with rp2gen.  rp2gen writes the following into
# the current build directory:
#   <name>.h <name>.client.c <name>.server.c
#   <name>.multi.c <name>.print.c <name>.helper.c
# rpc2[name]['header'|'client'|'server'|'multi'|'print'|'helper']
# holds the corresponding generated artifact.
#
# The generated code for every interface includes vcrcommon.h, so each
# generation depends on vcrcommon.rpc2, but the interfaces do not depend
# on each other.
#
# vcrcommon only defines shared datatypes, so rp2gen generates no client
# or server code for it - only the header and helper.
vcrcommon = custom_target('vcrcommon_rpc2',
  input : 'vcrcommon.rpc2',
  output : ['vcrcommon.h', 'vcrcommon.helper.c'],
  command : [rp2gen, '-C', '@OUTDIR@', '-I', '@CURRENT_SOURCE_DIR@', '@INPUT@'],
)

rpc2 = {
  'vcrcommon' : {
    'header' : vcrcommon[0],
    'helper' : vcrcommon[1],
  },
}
foreach f : rpc2_files
  base = f.split('.')[0]
  gen = custom_target(base + '_rpc2',
    input : f,
    output : [
      base + '.h',
      base + '.client.c',
      base + '.server.c',
      base + '.multi.c',
      base + '.print.c',
      base + '.helper.c',
    ],
    command : [rp2gen, '-C', '@OUTDIR@', '-I', '@CURRENT_SOURCE_DIR@', '@INPUT@'],
    depend_files : ['vcrcommon.rpc2'],
  )
  rpc2 = rpc2 + {
    base : {
      'header' : gen[0],
      'client' : gen[1],
      'server' : gen[2],
      'multi' : gen[3],
      'print' : gen[4],
      'helper' : gen[5],
    },
  }
endforeach

vicedep_inc = include_directories('.')

# libvenusdep, from Makefile.am
venusdep_lib = static_library('venusdep',
  [
    rpc2['callback']['helper'],
    rpc2['callback']['server'],
    rpc2['cml']['client'],
    rpc2['cml']['helper'],
    rpc2['vcrcommon']['header'],
    rpc2['vcrcommon']['helper'],
    rpc2['vice']['client'],
    rpc2['vice']['helper'],
    rpc2['vice']['multi'],
  ],
  build_by_default : false,
  include_directories : [config_h_include],
  dependencies : [rvm_rpc2_dep, rpc2_dep],
)

# libvicedep, from Makefile.am
vicedep_lib = static_library('vicedep',
  [
    rpc2['callback']['client'],
    rpc2['callback']['helper'],
    rpc2['callback']['multi'],
    rpc2['cml']['helper'],
    rpc2['mond']['client'],
    rpc2['mond']['helper'],
    rpc2['res']['client'],
    rpc2['res']['helper'],
    rpc2['res']['multi'],
    rpc2['res']['server'],
    rpc2['vcrcommon']['header'],
    rpc2['vcrcommon']['helper'],
    rpc2['vice']['helper'],
    rpc2['vice']['server'],
    rpc2['voldump']['client'],
    rpc2['voldump']['helper'],
    rpc2['volutil']['header'],
    rpc2['volutil']['helper'],
    rpc2['volutil']['server'],
  ],
  build_by_default : false,
  include_directories : [config_h_include],
  dependencies : [rvm_rpc2_dep, rpc2_dep],
)

# libvolutildep, from Makefile.am
volutildep_lib = static_library('volutildep',
  [
    rpc2['vcrcommon']['header'],
    rpc2['vcrcommon']['helper'],
    rpc2['voldump']['helper'],
    rpc2['voldump']['server'],
    rpc2['volutil']['client'],
    rpc2['volutil']['header'],
    rpc2['volutil']['helper'],
  ],
  build_by_default : false,
  include_directories : [config_h_include],
  dependencies : [rvm_rpc2_dep, rpc2_dep],
)

venusdep_dep = declare_dependency(
  include_directories : vicedep_inc,
  link_with : venusdep_lib,
)

vicedep_dep = declare_dependency(
  include_directories : vicedep_inc,
  link_with : vicedep_lib,
)

volutildep_dep = declare_dependency(
  include_directories : vicedep_inc,
  link_with : volutildep_lib,
)
