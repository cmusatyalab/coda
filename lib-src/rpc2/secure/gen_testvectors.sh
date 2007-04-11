#!/bin/sh
#
# Extract rijndael test values,
#	http://csrc.nist.gov/CryptoToolkit/aes/rijndael/rijndael-vals.zip
#
# Unzip them in a subdirectory 'testvalues' and run this script to build
# testvectors.h
#

#
# All included test values are checked whenever we initialize the security
# layer. By limiting the number of values we extract, we can reduce both the
# memory overhead to store the test values and time for checking them.
#

# Limit the number of included test vectors
# Maximum = 400,
# - Adds 52.5KB of test vector data
# - secure_init will run 4,000,960 encryption and 4,000,960 decryption cycles
#   and will compare the results against 3360 test values to check if the AES
#   implementation returns expected values.
# - 37 seconds on a 600MHz PIII, 7.0 seconds on a 3.2GHz P4
#limit=400

# Setting the limit to 40
# - Adds 7.5KB of test data
# - secure_init will run 400,240 encryption and 400,240 decryption cycles
#   comparing the results against 480 test values.
# - 3.8 seconds on a 600MHz PIII, 0.7 seconds on a 3.2GHz P4
#limit=40

# Setting the limit to 4
# - Adds 0.75KB of test data
# - secure_init will run 40,024 encryption and 40,024 decryption cycles
#   comparing the results against 48 test values.
# - 0.5 seconds on a 600MHz PIII, 0.15 seconds on a 3.2GHz P4
limit=4

vectors () {
    array=$1
    file=$2
    match=$3
    limit=$4
    IFS='='
    echo -n "$array " 1>&2
    echo "static const char $array[] ="
    I=0
    keysize=0
    cat $file | sed 's/\r//' | while read key value
    do
	if [ "$key" = KEYSIZE -a "$value" != "$keysize" ] ; then
	    I=0
	    keysize="$value"
	fi
	[ "$I" -ge $limit -o "$key" != $match ] && continue
	echo "	\""`echo $value | sed 's/\(..\)/\\\x\1/g'`"\""
	I=`expr $I + 1`
	[ "`expr $I % 20`"  -eq 0 ] && echo -n "." 1>&2
    done
    echo ";"
    echo " done" 1>&2
}

echo "generating testvectors.h " 1>&2
vectors aes_ecb_em testvalues/ecb_e_m.txt CT $limit > testvectors.h
vectors aes_ecb_dm testvalues/ecb_d_m.txt PT $limit >> testvectors.h
vectors aes_ecb_vt testvalues/ecb_vt.txt CT $limit >> testvectors.h
vectors aes_ecb_vk testvalues/ecb_vk.txt CT $limit >> testvectors.h

