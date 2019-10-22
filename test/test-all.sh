#!/usr/bin/env bash

make

for TEST in geometry_func_unittest math_func_unittest
do
    echo $TEST
    if ! ./${TEST}
    then
        exit 1
    fi
done

echo "All tests passed."
