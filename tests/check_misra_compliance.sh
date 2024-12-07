#!/bin/sh

cd ..

if ! test -f ./unicorn.c; then
    echo "missing 'unicorn.c' - please run 'unicorn.py' and try again"
    exit 1
fi

cppcheck --std=c99 --addon=misra --suppressions-list=tests/suppressions.txt --enable=all --force -D__cppcheck__ unicorn.c unicorn.h