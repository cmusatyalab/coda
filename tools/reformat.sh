#!/bin/sh
clang-format-6.0 -style=file -i $(git ls-files . | grep '\.[ch]c\?$')
