#!/bin/sh

set -e
# get absolute path to rvm_basher binary
basher="$(readlink -f "$1")"
rvmutl="$(readlink -f "$2")"
rdsinit="$(readlink -f "$3")"

# switch to directory containing test data
cd "$(dirname "$0")"

# cleanup from previous run
rm -f basher_log basher_data

$rvmutl << EOF
i basher_log 2M
q
EOF

$rdsinit -f basher_log basher_data 51208192 0x50000000 25600000 25600000 80 64

$basher << EOF
log basher_log
data basher_data
map_private
ops 500
cycles 2
threads 2
seed 12345
optimizations all
truncate 95
pre_allocate 100
block_size 10000
mod_size 200
max_big_range 500000
all_tests
parms
run
EOF

rm -f basher_log basher_data
