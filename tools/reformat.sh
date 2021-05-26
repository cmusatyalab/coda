#!/bin/sh
# run just the pre-commit hooks that reformat code
for hook in end-of-file-fixer trailing-whitespace clang-format isort black ; do
    pre-commit run --all-files $hook
done
