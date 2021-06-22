base_sources = [
  'base64.c',
  'base64.h',
  'coda_assert.c',
  'coda_assert.h',
  'coda_flock.c',
  'coda_flock.h',
  'coda_getaddrinfo.c',
  'coda_getaddrinfo.h',
  'coda_getservbyname.c',
  'coda_getservbyname.h',
  'coda_hash.h',
  'coda_largefile.h',
  'coda_malloc.c',
  'coda_offsetof.h',
  'coda_string.h',
  'coda_tsa.h',
  'coda_wait.h',
  'codaconf.c',
  'codaconf.h',
  'codaenv.c',
  'codaenv.h',
  'copyfile.c',
  'copyfile.h',
  'deprecations.h',
  'dllist.c',
  'dllist.h',
  'getpeereid.c',
  'getpeereid.h',
  'md5c.c',
  'parse_realms.c',
  'parse_realms.h',
  'sha1.c',
  'urlquote.c',
  'urlquote.h',
]

if readline_dep.found()
  base_sources += ['parser.c', 'parser.h']
endif

if not have_fts
  base_sources += ['coda_fts.c', 'coda_fts.h']
endif

base_lib = static_library('base',
  base_sources,
  include_directories : [config_h_include],
  dependencies : [rpc2_dep, readline_dep],
)

base_dep = declare_dependency(
  include_directories : [include_directories('.')],
  link_with : base_lib,
)

base_inc = include_directories('.')
