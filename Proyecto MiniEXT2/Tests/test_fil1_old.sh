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

# Function to check the existence of a file
check_file_exists() {
    if [ -f "$1" ]; then
        echo "Test passed: File $1 exists"
    else
        echo "Test failed: File $1 does not exist"
        exit 1
    fi
}

# Function to check the non-existence of a file
check_file_not_exists() {
    if [ ! -f "$1" ]; then
        echo "Test passed: File $1 does not exist"
    else
        echo "Test failed: File $1 exists"
        exit 1
    fi
}

# Function to check file content (with newline and whitespace trimming)
check_file_content() {
    file=$1
    expected_content=$2
    actual_content=$(<"$file")
    # Trim whitespace and newlines from both expected and actual content
    expected_content=$(echo -n "$expected_content" | tr -d '[:space:]')
    actual_content=$(echo -n "$actual_content" | tr -d '[:space:]')
    if [ "$actual_content" = "$expected_content" ]; then
        echo "Test passed: Content of $file is correct"
    else
        echo "Test failed: Content of $file is incorrect"
        echo "Expected:"
        echo "$expected_content"
        echo "Actual:"
        echo "$actual_content"
        exit 1
    fi
}

# Test file operations
test_files() {
    local test_dir=$1
    echo "Testing file operations in directory: $test_dir"

    # Create a file
    echo "Creating a file..."
    echo "Hello, this is a test file." > "$test_dir/test_file.txt"
    check_result $? "Creating $test_dir/test_file.txt"
    check_file_exists "$test_dir/test_file.txt"
    check_file_content "$test_dir/test_file.txt" "Hello, this is a test file."

    # Read from a file
    echo "Reading from the file..."
    content=$(<"$test_dir/test_file.txt")
    check_result $? "Reading from $test_dir/test_file.txt"
    check_file_content "$test_dir/test_file.txt" "Hello, this is a test file."

    # Write to a file (overwrite)
    echo "Writing to the file (overwrite)..."
    echo "This will overwrite the existing content." > "$test_dir/test_file.txt"
    check_result $? "Writing to $test_dir/test_file.txt"
    check_file_content "$test_dir/test_file.txt" "This will overwrite the existing content."

    # Append to a file
    echo "Appending to the file..."
    echo -n "This will be appended." >> "$test_dir/test_file.txt"
    check_result $? "Appending to $test_dir/test_file.txt"
    check_file_content "$test_dir/test_file.txt" "This will overwrite the existing content.This will be appended."

    # Copy a file
    echo "Copying the file..."
    cp "$test_dir/test_file.txt" "$test_dir/test_file_copy.txt"
    check_result $? "Copying $test_dir/test_file.txt to $test_dir/test_file_copy.txt"
    check_file_exists "$test_dir/test_file_copy.txt"
    check_file_content "$test_dir/test_file_copy.txt" "This will overwrite the existing content.This will be appended."

    # Move a file
    echo "Moving the file..."
    mv "$test_dir/test_file.txt" "$test_dir/test_file_moved.txt"
    check_result $? "Moving $test_dir/test_file.txt to $test_dir/test_file_moved.txt"
    check_file_exists "$test_dir/test_file_moved.txt"
    check_file_not_exists "$test_dir/test_file.txt"
    check_file_content "$test_dir/test_file_moved.txt" "This will overwrite the existing content.This will be appended."

    # Truncate a file
    echo "Truncating the file..."
    truncate -s 0 "$test_dir/test_file_moved.txt"
    check_result $? "Truncating $test_dir/test_file_moved.txt"
    check_file_content "$test_dir/test_file_moved.txt" ""

    # Delete a file
    echo "Deleting the file..."
    rm "$test_dir/test_file_moved.txt"
    check_result $? "Deleting $test_dir/test_file_moved.txt"
    check_file_not_exists "$test_dir/test_file_moved.txt"

    echo "File operations in directory $test_dir complete."
}

# Clean up function to ensure a fresh start
cleanup() {
    local test_dir=$1
    echo "Cleaning up..."
    rm -f "$test_dir/test_file.txt"
    rm -f "$test_dir/test_file_copy.txt"
    rm -f "$test_dir/test_file_moved.txt"
    echo "Cleanup complete."
}

# Main function to run all tests
main() {
    if [ -z "$1" ]; then
        echo "Usage: $0 <test_directory>"
        exit 1
    fi

    local test_dir=$1
    cleanup "$test_dir"
    test_files "$test_dir"
    cleanup "$test_dir"
    echo "All tests complete."
}

# Run the main function with the provided directory
main "$@"
