#!/usr/bin/env python
#
# Find functions that are exported but not used, which may be an indication of
# dead code.  Should be run from the top of the Coda build tree otherwise it
# will generate many false positives because it won't find all possible places
# where functions are used.
#
# The discovered functions may be used in the file where they are defined. Once
# they are declared as 'static' the compiler will warn if they are really
#
# Usage: find_unused_symbols.py
#
from __future__ import print_function

import argparse
import os
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--all", action='store_true',
                    help="include object files that define a 'main' function.")
parser.add_argument("root", nargs='?', default=".",
                    help="root of build tree with object files.")


def demangle(names):
    """Turn mangled C++ names back into something humans can understand."""
    if not names:
        return []
    output = subprocess.check_output(['c++filt'] + list(names))
    demangled = [ name for name in output.split('\n') if name ]
    return demangled


def extract_symbols(filename):
    """Extract defined/used function names from object files"""
    defined, used = set(), set()

    output = subprocess.check_output(['nm', filename])
    for sym in output.split('\n'):
        if not sym:
            continue
        addr, t, symbol = sym[:16], sym[17], sym[19:]

        if t == 'T':
            defined.add(symbol)

        elif t == 'U':
            used.add(symbol)

    return demangle(defined), demangle(used)


def enumerate_objects(root):
    """Walk tree from root and return names of object files."""
    for root, dirs, files in os.walk(root):
        if '.libs' in dirs:
            dirs.remove('.libs')

        for name in files:
            if not name.endswith('.o'):
                continue

            yield os.path.join(root, name)


if __name__ == '__main__':
    args = parser.parse_args()

    syms_defined = {}
    syms_used = set()

    # collect all defined and used symbols from the build tree
    for filename in enumerate_objects(args.root):
        defined, used = extract_symbols(filename)

        for symbol in used:
            syms_used.add(symbol)

        if 'main' in defined:
            if not args.all:
                continue
            defined.remove('main')

        for symbol in defined:
            syms_defined.setdefault(symbol, []).append(filename)

    # create dict containing all files with unused symbols
    unused = {}
    for symbol, files in syms_defined.items():
        if symbol not in syms_used:
            for filename in files:
                unused.setdefault(filename, []).append(symbol)

    # pretty print results
    for filename in sorted(unused):
        print(filename)
        for symbol in sorted(unused[filename]):
            print("    {}".format(symbol))
        print()

