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

# Function to generate a large test file (~6KB)
create_large_file() {
    local file=$1
    local size_kb=6
    echo "Generating $size_kb KB file: $file"
    head -c "${size_kb}KB" /dev/urandom > "$file"
    check_result $? "Generating $size_kb KB file: $file"
    check_file_exists "$file"
}

# Function to calculate checksum (SHA-256) of a file
calculate_checksum() {
    local file=$1
    sha256sum "$file" | awk '{print $1}'
}

# Function to check file content (using checksum)
check_file_content_checksum() {
    file=$1
    expected_checksum=$2
    actual_checksum=$(calculate_checksum "$file")
    if [ "$actual_checksum" = "$expected_checksum" ]; then
        echo "Test passed: Checksum of $file is correct"
    else
        echo "Test failed: Checksum of $file is incorrect"
        echo "Expected checksum: $expected_checksum"
        echo "Actual checksum: $actual_checksum"
        exit 1
    fi
}

# Test large file operations
test_large_files() {
    local test_dir=$1
    echo "Testing large file operations in directory: $test_dir"

    # Create a large file
    create_large_file "$test_dir/large_file.bin"

    #Copy the large file
    echo "Copying the large file..."
    cp "$test_dir/large_file.bin" "$test_dir/large_file_copy.bin"
    check_result $? "Copying $test_dir/large_file.bin to $test_dir/large_file_copy.bin"
    check_file_exists "$test_dir/large_file_copy.bin"

    #Check checksum of original and copied large files
    original_checksum=$(calculate_checksum "$test_dir/large_file.bin")
    check_file_content_checksum "$test_dir/large_file_copy.bin" "$original_checksum"
    
    #Move the large file
    echo "Moving the large file..."
    mv "$test_dir/large_file.bin" "$test_dir/large_file_moved.bin"
    check_result $? "Moving $test_dir/large_file.bin to $test_dir/large_file_moved.bin"
    check_file_exists "$test_dir/large_file_moved.bin"
    check_file_not_exists "$test_dir/large_file.bin"

    # Check checksum after move operation
    check_file_content_checksum "$test_dir/large_file_moved.bin" "$original_checksum"

    # Append to the large file
    echo "Appending to the large file..."
    echo "Additional content appended." >> "$test_dir/large_file_moved.bin"
    check_result $? "Appending to $test_dir/large_file_moved.bin"

    # Check checksum after append operation
    updated_checksum=$(calculate_checksum "$test_dir/large_file_moved.bin")
    if [ "$updated_checksum" != "$original_checksum" ]; then
        echo "Test passed: Checksum of $test_dir/large_file_moved.bin is updated after append"
    else
        echo "Test failed: Checksum of $test_dir/large_file_moved.bin is not updated after append"
        echo "Original checksum: $original_checksum"
        echo "Current checksum: $updated_checksum"
        exit 1
    fi
    #Clean up large files
    #rm -f "$test_dir/large_file.bin"
    #rm -f "$test_dir/large_file_copy.bin"
    #rm -f "$test_dir/large_file_moved.bin"

    echo "Large file operations in directory $test_dir complete."
}

# Clean up function to ensure a fresh start
cleanup() {
    local test_dir=$1
    echo "Cleaning up..."
    rm -f "$test_dir/large_file.bin"
    rm -f "$test_dir/large_file_copy.bin"
    rm -f "$test_dir/large_file_moved.bin"
    echo "Cleanup complete."
}

# Main function to run all tests
main() {
    if [ -z "$1" ]; then
        echo "Usage: $0 <test_directory>"
        exit 1
    fi

    local test_dir=$1
    #cleanup "$test_dir"
    test_large_files "$test_dir"
    #cleanup "$test_dir"
    echo "All tests complete."
}

# Run the main function with the provided directory
main "$@"
