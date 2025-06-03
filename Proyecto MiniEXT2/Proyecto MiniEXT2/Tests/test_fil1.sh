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

# Function to check the existence of a symlink
check_symlink_exists() {
    if [ -L "$1" ]; then
        echo "Test passed: Symlink $1 exists"
    else
        echo "Test failed: Symlink $1 does not exist"
        exit 1
    fi
}

# Function to check file content (whitespace trimmed)
check_file_content() {
    file=$1
    expected_content=$2
    actual_content=$(<"$file")
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

# Function to test symbolic and hard links, ownership, and group changes
test_links_and_ownership() {
    local test_dir=$1
    echo "Testing links and ownership..."

    # Create a base file
    echo "Original file content" > "$test_dir/original.txt"
    check_file_exists "$test_dir/original.txt"

    # Create a symbolic link
    ln -s "$test_dir/original.txt" "$test_dir/symlink.txt"
    check_result $? "Creating symbolic link"
    check_symlink_exists "$test_dir/symlink.txt"
    check_file_content "$test_dir/symlink.txt" "Original file content"

    # Create a hard link
    ln "$test_dir/original.txt" "$test_dir/hardlink.txt"
    check_result $? "Creating hard link"
    check_file_exists "$test_dir/hardlink.txt"
    check_file_content "$test_dir/hardlink.txt" "Original file content"

    # Modify the original file, both links should reflect change
    echo "Modified content" > "$test_dir/original.txt"
    check_file_content "$test_dir/hardlink.txt" "Modified content"
    check_file_content "$test_dir/symlink.txt" "Modified content"

    # Change ownership and group (requires sudo)
    echo "Changing ownership and group..."
    user=$(logname)
    group=$(id -gn "$user")
    chown "$root:$root" "$test_dir/original.txt"
    check_result $? "Changing ownership to $root:$root"
    chown "$user:$group" "$test_dir/original.txt"
    check_result $? "Changing ownership to $user:$group"
    file_user=$(stat -c "%U" "$test_dir/original.txt")
    file_group=$(stat -c "%G" "$test_dir/original.txt")
    if [ "$file_user" = "$user" ] && [ "$file_group" = "$group" ]; then
        echo "Test passed: Ownership and group correctly set to $user:$group"
    else
        echo "Test failed: Incorrect owner/group: $file_user:$file_group"
        exit 1
    fi


    # Cleanup links
    #rm -f "$test_dir/original.txt" "$test_dir/symlink.txt" "$test_dir/hardlink.txt"
    echo "Links and ownership tests complete."
}

# Clean up function
cleanup() {
    local test_dir=$1
    echo "Cleaning up..."
    rm -f "$test_dir/test_file.txt" "$test_dir/test_file_copy.txt" "$test_dir/test_file_moved.txt"
    rm -f "$test_dir/original.txt" "$test_dir/symlink.txt" "$test_dir/hardlink.txt"
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
    test_files "$test_dir"
    test_links_and_ownership "$test_dir"
    #cleanup "$test_dir"
    echo "All tests complete."
}

# Test file operations (already provided)
test_files() {
    local test_dir=$1
    echo "Testing file operations in directory: $test_dir"

    echo "Creating a file..."
    echo "Hello, this is a test file." > "$test_dir/test_file.txt"
    check_result $? "Creating $test_dir/test_file.txt"
    check_file_exists "$test_dir/test_file.txt"
    check_file_content "$test_dir/test_file.txt" "Hello, this is a test file."

    echo "Reading from the file..."
    content=$(<"$test_dir/test_file.txt")
    check_result $? "Reading from $test_dir/test_file.txt"
    check_file_content "$test_dir/test_file.txt" "Hello, this is a test file."

    echo "Writing to the file (overwrite)..."
    echo "This will overwrite the existing content." > "$test_dir/test_file.txt"
    check_result $? "Writing to $test_dir/test_file.txt"
    check_file_content "$test_dir/test_file.txt" "This will overwrite the existing content."

    echo "Appending to the file..."
    echo -n "This will be appended." >> "$test_dir/test_file.txt"
    check_result $? "Appending to $test_dir/test_file.txt"
    check_file_content "$test_dir/test_file.txt" "This will overwrite the existing content.This will be appended."

    echo "Copying the file..."
    cp "$test_dir/test_file.txt" "$test_dir/test_file_copy.txt"
    check_result $? "Copying $test_dir/test_file.txt to $test_dir/test_file_copy.txt"
    check_file_exists "$test_dir/test_file_copy.txt"
    check_file_content "$test_dir/test_file_copy.txt" "This will overwrite the existing content.This will be appended."

    echo "Moving the file..."
    mv "$test_dir/test_file.txt" "$test_dir/test_file_moved.txt"
    check_result $? "Moving $test_dir/test_file.txt to $test_dir/test_file_moved.txt"
    check_file_exists "$test_dir/test_file_moved.txt"
    check_file_not_exists "$test_dir/test_file.txt"
    check_file_content "$test_dir/test_file_moved.txt" "This will overwrite the existing content.This will be appended."

    echo "Truncating the file..."
    truncate -s 0 "$test_dir/test_file_moved.txt"
    check_result $? "Truncating $test_dir/test_file_moved.txt"
    check_file_content "$test_dir/test_file_moved.txt" ""

    echo "Deleting the file..."
    rm "$test_dir/test_file_moved.txt"
    check_result $? "Deleting $test_dir/test_file_moved.txt"
    check_file_not_exists "$test_dir/test_file_moved.txt"

    echo "File operations in directory $test_dir complete."
}

# Run main
main "$@"
