#!/bin/bash

# Function to check the result of a command
check_result() {
    if [ $1 -ne 0 ]; then
        echo "Test failed: $2"
        exit 1
    else
        echo "Test passed: $2"
    fi
}

# Function to check the existence of a directory
check_dir_exists() {
    if [ -d "$1" ]; then
        echo "Test passed: Directory $1 exists"
    else
        echo "Test failed: Directory $1 does not exist"
        exit 1
    fi
}

# Function to check the non-existence of a directory
check_dir_not_exists() {
    if [ ! -d "$1" ]; then
        echo "Test passed: Directory $1 does not exist"
    else
        echo "Test failed: Directory $1 exists"
        exit 1
    fi
}

# Function to print the directory tree
print_tree() {
    echo "Current directory structure:"
    tree .
    echo
}

# Test directory operations
test_directories() {
    echo "Testing directory operations..."

    # Create directories
    echo "Creating directories..."
    mkdir test_dir
    check_result $? "Creating test_dir"
    mkdir test_dir/subdir
    check_result $? "Creating test_dir/subdir"
    mkdir -p test_dir/subdir2/subsubdir
    check_result $? "Creating test_dir/subdir2/subsubdir"
    print_tree

    # Rename directories
    echo "Renaming directories..."
    mv test_dir/subdir test_dir/subdir_renamed
    check_result $? "Renaming test_dir/subdir to test_dir/subdir_renamed"
    check_dir_exists test_dir/subdir_renamed
    check_dir_not_exists test_dir/subdir
    print_tree

    # Rename directories to the same name (no-op)
    echo "Renaming directories to the same name (no-op)..."
    if [ "test_dir/subdir_renamed" != "test_dir/subdir_renamed" ]; then
        mv test_dir/subdir_renamed test_dir/subdir_renamed
        check_result $? "Renaming test_dir/subdir_renamed to the same name"
    else
        echo "Test passed: No-op renaming test_dir/subdir_renamed to the same name"
    fi
    check_dir_exists test_dir/subdir_renamed
    print_tree

    # Move directories
    echo "Moving directories..."
    mv test_dir/subdir_renamed test_dir/subdir_moved
    check_result $? "Moving test_dir/subdir_renamed to test_dir/subdir_moved"
    check_dir_exists test_dir/subdir_moved
    check_dir_not_exists test_dir/subdir_renamed
    print_tree

    # Move a branch with children
    echo "Moving a branch with children..."
    mkdir -p test_dir/branch/child
    mv test_dir/branch test_dir/branch_moved
    check_result $? "Moving test_dir/branch to test_dir/branch_moved"
    check_dir_exists test_dir/branch_moved
    check_dir_exists test_dir/branch_moved/child
    check_dir_not_exists test_dir/branch
    print_tree

    # Delete directories
    echo "Deleting directories..."
    #rm -rf test_dir
    check_result $? "Deleting test_dir"
    check_dir_not_exists test_dir
    print_tree

    echo "Directory operations complete."
}

# Clean up function to ensure a fresh start
cleanup() {
    echo "Cleaning up..."
    rm -rf test_dir 2>/dev/null
    rm -rf test_dir/subdir_existing 2>/dev/null
    rm -rf test_dir/branch_moved 2>/dev/null
    echo "Cleanup complete."
}

# Main function to run all tests
main() {
    if [ -z "$1" ]; then
        echo "Usage: $0 <root_directory>"
        exit 1
    fi

    cd "$1" || { echo "Failed to change directory to $1"; exit 1; }

    #cleanup
    test_directories
    #cleanup
    echo "All tests complete."
}

# Run the main function with the provided directory
main "$@"
