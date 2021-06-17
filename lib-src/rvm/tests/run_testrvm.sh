#!/bin/sh

set -e
# get absolute path to testrvm binary
testrvm="$(readlink -f "$1")"

# switch to directory containing test data
cd "$(dirname "$0")"

# cleanup from possibly failed previous run
rm -f log_file

echo "*** Tests with data segment copied in memory"
$testrvm << EOF
n
n
y
y
EOF
rm -f log_file

echo "*** Abort test with a crash"
$testrvm << EOF || true
n
n
y
n
y
EOF
echo

echo "*** Test recovery from crash"
$testrvm << EOF
n
y
EOF
rm -f log_file

echo "*** Tests with data segment privately mmapped"
$testrvm << EOF
y
n
y
y
EOF
rm -f log_file
