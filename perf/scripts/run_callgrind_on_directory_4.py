#!/usr/bin/env python3

import sys
from pathlib import Path
import os
import subprocess # Module to run external commands
import time       # Module for timing execution

def find_sif_files_recursively(directory_path_str: str) -> list[str]:
    """
    Recursively finds all files ending with '.sif' in the given directory.

    Args:
        directory_path_str: The path to the directory to search.

    Returns:
        A list of absolute paths to the found '.sif' files.
        Returns an empty list if the directory is invalid or not found.
    """
    sif_files_absolute = []
    directory_path = Path(directory_path_str)

    # Validate the input directory path
    if not directory_path.exists():
        print(f"Error: Directory not found: {directory_path_str}", file=sys.stderr)
        return []
    if not directory_path.is_dir():
        print(f"Error: Provided path is not a directory: {directory_path_str}", file=sys.stderr)
        return []

    # Use rglob to recursively find all files matching the pattern '*.sif'
    # rglob yields Path objects
    print(f"Searching for .sif files in: {directory_path.resolve()}...")
    for file_path in directory_path.rglob('*.sif'):
        # Although rglob typically yields files, double-check it's a file
        if file_path.is_file():
            # Get the absolute path (resolve symlinks and make absolute)
            # Convert the Path object to a string for the final list
            sif_files_absolute.append(str(file_path.resolve()))

    return sif_files_absolute

def main():
    """
    Main function to handle command-line arguments, find .sif files,
    run an executable on each found file (timed), suppressing the executable's output.
    """
    # Check if command-line arguments (executable path and directory) were provided
    # Arguments expected: script_name, executable_path, search_directory
    if len(sys.argv) < 3:
        # Updated Usage message
        print(f"Usage: {sys.argv[0]} <executable_path> <search_directory>", file=sys.stderr)
        sys.exit(1) # Exit with an error code

    # Get the executable path (ARG 1) and directory path (ARG 2)
    executable_path_str = sys.argv[1]
    target_directory = sys.argv[2]

    # --- Validate Executable Path (Now Arg 1) ---
    exe_path = Path(executable_path_str)
    if not exe_path.exists():
        print(f"Error: Executable not found: {executable_path_str}", file=sys.stderr)
        sys.exit(1)
    if not exe_path.is_file():
        print(f"Error: Executable path is not a file: {executable_path_str}", file=sys.stderr)
        sys.exit(1)
    # Check for execute permissions
    if not os.access(str(exe_path), os.X_OK):
        print(f"Error: Provided path is not executable: {executable_path_str}", file=sys.stderr)
        sys.exit(1)

    # --- Find the .sif files (using Arg 2) ---
    # The find function itself doesn't need changing, just the argument passed to it
    found_files = find_sif_files_recursively(target_directory)

    # --- Execute the command for each found file ---
    if found_files:
        print(f"\nFound {len(found_files)} .sif files. Running executable '{exe_path.resolve()}' on each (output suppressed)...")
        executable_abs_path = str(exe_path.resolve()) # Use absolute path for subprocess

        processed_count = 0
        error_count = 0
        total_processing_time_start = time.perf_counter() # Start total timer

        for sif_file_path in found_files:
            # Construct the command as a list of arguments
            command = [executable_abs_path, sif_file_path]
            # Print message before starting, use end="" to append result later
            print(f"---> Processing: {sif_file_path}", end="")
            start_time = time.perf_counter() # Start timer for this file
            duration = 0.0 # Initialize duration

            try:
                # Run the command, redirecting stdout and stderr to DEVNULL
                # check=False allows us to check the return code manually.
                result = subprocess.run(
                    command,
                    check=False,
                    stdout=subprocess.DEVNULL, # Suppress standard output
                    stderr=subprocess.DEVNULL  # Suppress standard error
                )
                # Calculate duration after the command finishes
                duration = time.perf_counter() - start_time

                if result.returncode == 0:
                    # Append success status and duration
                    print(f" [Success] ({duration:.1f} s)")
                    processed_count += 1
                else:
                    # Append failure status and duration
                    print(f" [Failed - Code: {result.returncode}] ({duration:.1f} s)")
                    # Optionally log more details to stderr if needed
                    # print(f"     Error details logged for: {sif_file_path}", file=sys.stderr)
                    error_count += 1

            except OSError as e:
                 # Calculate duration up to the point of error if possible
                 duration = time.perf_counter() - start_time
                 # Append OS error status and duration (might be very short)
                 print(f" [OS Error: {e}] ({duration:.1f} s)")
                 error_count += 1
            except Exception as e:
                 # Calculate duration up to the point of error if possible
                 duration = time.perf_counter() - start_time
                 # Append unexpected error status and duration
                 print(f" [Unexpected Error: {e}] ({duration:.1f} s)")
                 error_count += 1

        total_processing_time_end = time.perf_counter() # Stop total timer
        total_duration = total_processing_time_end - total_processing_time_start

        print("\n--- Summary ---")
        print(f"Total .sif files found: {len(found_files)}")
        print(f"Attempted processing: {processed_count + error_count}")
        print(f"Succeeded (exit code 0): {processed_count}")
        print(f"Failed (non-zero exit or error): {error_count}")
        print(f"Total processing loop time: {total_duration:.2f} s") # Added total time

    else:
        # Only print if the find function didn't already print an error
        # Validate the target directory path *before* printing "No .sif files found"
        # to avoid confusion if the directory itself was invalid.
        dir_path_obj = Path(target_directory)
        if dir_path_obj.exists() and dir_path_obj.is_dir():
             print("\nNo .sif files found in the specified directory.")
        # Error messages for invalid directory are handled in find_sif_files_recursively

# Standard Python entry point guard
if __name__ == "__main__":
    main()

