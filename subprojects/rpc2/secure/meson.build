secure_sources = [
  'aes.h',
  'auth_aes_xcbc.c',
  'auth_none.c',
  'encr_aes_cbc.c',
  'encr_aes_ccm.c',
  'encr_null.c',
  'grunt.h',
  'rijndael-alg-fst.c',
  'rijndael-alg-fst.h',
  'secure_aes.c',
  'secure_init.c',
  'secure_pbkdf.c',
  'secure_random.c',
  'secure_recvfrom.c',
  'secure_sendto.c',
  'secure_setup.c',
  'testvectors.h',
]

secure_lib = static_library('secure',
  secure_sources,
  include_directories : [config_h_include, rpc2_inc],
)

secure_dep = declare_dependency(
  include_directories : rpc2_inc,
  link_with : secure_lib,
)

test = executable('secure_test', 'secure_test.c', dependencies : [secure_dep])
test('secure_test', test)
