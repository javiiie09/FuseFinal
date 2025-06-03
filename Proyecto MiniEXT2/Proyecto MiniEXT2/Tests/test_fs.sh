#!/bin/bash


# Get the directory of the current script
script_dir=$(dirname "$(readlink -f "$0")")

if [ -z "$1" ]; then
    echo "Usage: $0 <test_directory>"
    exit 1
fi

test_dir=$(readlink -f "$1")

# Change directory to the location of test scripts
cd "$script_dir" || exit


echo "-------------------- Running tests on $test_dir"
./test_dir1.sh "$test_dir"
./test_fil1.sh "$test_dir"
./test_fil2.sh "$test_dir"
