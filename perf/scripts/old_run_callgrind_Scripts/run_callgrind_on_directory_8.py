#!/usr/bin/env python3

import sys
from pathlib import Path
import os
import subprocess # Module to run external commands
import time       # Module for timing execution
from dataclasses import dataclass # To create data classes
from typing import List, Optional, Set # Added Set for type hinting
import csv        # Module for CSV file operations

# --- Configuration ---
CSV_FILENAME = "processing_results.csv"

@dataclass
class ProcessingResult:
    """Holds the result of processing a single .sif file."""
    filepath: str
    status: str # e.g., "Success", "Failed", "OS Error", "Unexpected Error"
    return_code: Optional[int] # None if subprocess didn't finish
    duration_seconds: float

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
    found_paths = list(directory_path.rglob('*.sif')) # Collect paths first to get count
    print(f"Found {len(found_paths)} potential .sif files.")

    for file_path in found_paths:
        # Although rglob typically yields files, double-check it's a file
        if file_path.is_file():
            # Get the absolute path (resolve symlinks and make absolute)
            # Convert the Path object to a string for the final list
            sif_files_absolute.append(str(file_path.resolve()))

    return sif_files_absolute

def append_result_to_csv(result: ProcessingResult, filename: str):
    """Appends a single ProcessingResult to the specified CSV file."""
    file_exists = Path(filename).exists()
    is_empty = not file_exists or os.path.getsize(filename) == 0
    try:
        # Open file in append mode ('a'). newline='' is important for csv writer.
        with open(filename, mode='a', newline='', encoding='utf-8') as csvfile:
            # Define field names matching the ProcessingResult attributes
            fieldnames = ['filepath', 'status', 'return_code', 'duration_seconds']
            writer = csv.writer(csvfile)

            # Write header only if the file is empty
            if is_empty:
                writer.writerow(fieldnames) # Use fieldnames as header

            # Write the data row
            writer.writerow([
                result.filepath,
                result.status,
                result.return_code if result.return_code is not None else '', # Write empty string for None
                f"{result.duration_seconds:.3f}" # Format duration consistently
            ])
    except IOError as e:
        print(f"\nError writing to CSV file {filename}: {e}", file=sys.stderr)
    except Exception as e:
        print(f"\nUnexpected error writing to CSV {filename}: {e}", file=sys.stderr)

def read_processed_files(filename: str) -> Set[str]:
    """Reads the 'filepath' column from the CSV and returns a set of paths."""
    processed_paths: Set[str] = set()
    if not Path(filename).exists():
        print(f"Results file '{filename}' not found. Starting fresh.")
        return processed_paths # Return empty set if file doesn't exist

    try:
        with open(filename, mode='r', newline='', encoding='utf-8') as csvfile:
            reader = csv.DictReader(csvfile)
            # Check if the required header is present
            if 'filepath' not in reader.fieldnames:
                 print(f"Warning: CSV file '{filename}' is missing 'filepath' header. Cannot determine processed files.", file=sys.stderr)
                 return processed_paths # Return empty set if header is missing

            for row in reader:
                # Handle potential missing 'filepath' key in a specific row, though DictReader usually ensures consistency
                if 'filepath' in row and row['filepath']:
                    # Assume paths in CSV are absolute and resolved, matching find_sif_files_recursively output
                    processed_paths.add(row['filepath'])
        print(f"Read {len(processed_paths)} previously processed file paths from '{filename}'.")
    except FileNotFoundError:
         # Should not happen due to the initial check, but handle defensively
         print(f"Results file '{filename}' not found during read attempt. Starting fresh.")
    except Exception as e:
        print(f"\nError reading CSV file {filename}: {e}. Processing all found files.", file=sys.stderr)
        return set() # Return empty set on error to avoid incorrect skipping

    return processed_paths


def main():
    """
    Main function to handle command-line arguments, find .sif files,
    run an executable on each found file (timed), suppressing the executable's output,
    collecting results, logging results incrementally to CSV, and skipping already processed files.
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

    # --- Read previously processed files ---
    already_processed_paths = read_processed_files(CSV_FILENAME)

    # --- Find the .sif files (using Arg 2) ---
    # The find function itself doesn't need changing, just the argument passed to it
    found_files = find_sif_files_recursively(target_directory)

    # --- Execute the command for each found file and collect results ---
    results_list: List[ProcessingResult] = [] # Initialize list to store results *for this run*
    skipped_count = 0 # Initialize skipped counter

    if found_files:
        total_files_count = len(found_files) # Get total count for formatting
        # Calculate padding width based on the number of digits in the total count
        width = len(str(total_files_count))

        print(f"\nProcessing {total_files_count} .sif files using '{exe_path.resolve()}' (output suppressed)...")
        print(f"Results will be logged incrementally to: {CSV_FILENAME}")
        executable_abs_path = str(exe_path.resolve()) # Use absolute path for subprocess

        total_processing_time_start = time.perf_counter() # Start total timer

        # Use enumerate starting from 1 for the counter
        for idx, sif_file_path in enumerate(found_files, start=1):
            # Format the counter string with zero padding
            counter_str = f"{idx:0{width}}/{total_files_count}"

            # --- Check if file was already processed ---
            if sif_file_path in already_processed_paths:
                print(f"---> [{counter_str}] Skipping (already processed): {sif_file_path}")
                skipped_count += 1
                continue # Move to the next file

            # --- Process the file ---
            # Initialize variables for this iteration's result
            status: str = "Unknown Error"
            return_code: Optional[int] = None
            duration: float = 0.0
            result_obj: Optional[ProcessingResult] = None # Define here for broader scope

            # Print message before starting, including the counter
            print(f"---> [{counter_str}] Processing: {sif_file_path}", end="")
            sys.stdout.flush() # Flush output buffer
            start_time = time.perf_counter() # Start timer for this file

            try:
                # Run the command, redirecting stdout and stderr to DEVNULL
                # The command list is the FIRST positional argument
                result = subprocess.run(
                    [executable_abs_path, sif_file_path], # Pass command list positionally
                    check=False,
                    stdout=subprocess.DEVNULL, # Suppress standard output
                    stderr=subprocess.DEVNULL  # Suppress standard error
                )
                # Calculate duration after the command finishes
                duration = time.perf_counter() - start_time
                return_code = result.returncode

                if result.returncode == 0:
                    status = "Success"
                    print(f" [{status}] ({duration:.1f} s)")
                else:
                    status = "Failed"
                    print(f" [{status} - Code: {return_code}] ({duration:.1f} s)")

                result_obj = ProcessingResult(sif_file_path, status, return_code, duration)

            except OSError as e:
                 duration = time.perf_counter() - start_time
                 status = "OS Error"
                 print(f" [{status}: {e}] ({duration:.1f} s)")
                 result_obj = ProcessingResult(sif_file_path, status, None, duration) # No return code
            except Exception as e:
                 duration = time.perf_counter() - start_time
                 status = "Unexpected Error"
                 print(f" [{status}: {e}] ({duration:.1f} s)")
                 result_obj = ProcessingResult(sif_file_path, status, None, duration) # No return code

            # Append the result object to the list AND write to CSV
            if result_obj: # Ensure result_obj was created
                 results_list.append(result_obj)
                 append_result_to_csv(result_obj, CSV_FILENAME) # Log result immediately

        total_processing_time_end = time.perf_counter() # Stop total timer
        total_duration = total_processing_time_end - total_processing_time_start

        # --- Generate Summary from collected results ---
        print("\n--- Summary ---")
        total_files_processed_this_run = len(results_list)
        # Calculate counts based on the results *from this run*
        success_count = sum(1 for r in results_list if r.status == "Success")
        fail_count = total_files_processed_this_run - success_count # All non-Success states are failures

        print(f"Total .sif files found: {total_files_count}")
        print(f"Skipped (previously processed): {skipped_count}")
        print(f"Attempted processing this run: {total_files_processed_this_run}")
        print(f"Succeeded this run (exit code 0): {success_count}")
        print(f"Failed this run (non-zero exit or error): {fail_count}")
        print(f"Total processing loop time: {total_duration:.2f} s")
        print(f"Results logged to: {CSV_FILENAME}")

    else:
        # Only print if the find function didn't already print an error
        dir_path_obj = Path(target_directory)
        if dir_path_obj.exists() and dir_path_obj.is_dir():
             print("\nNo .sif files found in the specified directory.")
        # Error messages for invalid directory are handled in find_sif_files_recursively

# Standard Python entry point guard
if __name__ == "__main__":
    main()

