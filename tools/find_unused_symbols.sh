#!/bin/bash
#
# Find functions that are exported but not used, which may be an indication of
# dead code.  Should be run from the top of the Coda build tree otherwise it
# will generate many false positives.
#
# Usage: find_unused_symbols [path/to/objects]
#

find ${1:-.} -name '*.o' -exec nm {} \; | grep " T " | cut -c 20- | sort | uniq > symbols.defined
find . -name '*.o' -exec nm {} \; | grep " U " | cut -c 20- | sort | uniq > symbols.used
comm -2 -3 symbols.defined symbols.used | c++filt
rm symbols.defined symbols.used
