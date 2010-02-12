#!/bin/sh

../rvm/rvmutl >/dev/null << EOF
i basher_log 2M
q
EOF

../rds/rdsinit -f basher_log basher_data 51208192 0x50000000 \
    25600000 25600000 80 32

