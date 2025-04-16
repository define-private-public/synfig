#!/usr/bin/env python3

import sys
from pathlib import Path
import os
import subprocess # Module to run external commands
import time       # Module for timing execution
from dataclasses import dataclass # To create data classes
from typing import List, Optional, Set, Tuple, Dict # Added Dict for type hinting
import csv        # Module for CSV file operations

# --- Configuration ---
CSV_FILENAME = "processing_results.csv"

@dataclass
class ProcessingResult:
    """Holds the result of processing a single .sif file."""
    filepath: str # Now stores path relative to search directory
    status: str # e.g., "Success", "Failed", "OS Error", "Unexpected Error"
    return_code: Optional[int] # None if subprocess didn't finish
    duration_seconds: float

def find_sif_files_recursively(directory_path_str: str) -> list[str]:
    """
    Recursively finds all files ending with '.sif' in the given directory.

    Args:
        directory_path_str: The path to the directory to search.

    Returns:
        A list of paths relative to the search directory for the found '.sif' files.
        Returns an empty list if the directory is invalid or not found.
    """
    sif_files_relative = [] # Changed variable name for clarity
    try:
        # Resolve the directory path once to handle relative inputs cleanly
        directory_path = Path(directory_path_str).resolve(strict=True)
    except FileNotFoundError:
        print(f"Error: Directory not found: {directory_path_str}", file=sys.stderr)
        return []
    except Exception as e: # Catch other potential errors like permission issues
        print(f"Error accessing directory {directory_path_str}: {e}", file=sys.stderr)
        return []

    if not directory_path.is_dir():
        print(f"Error: Provided path is not a directory: {directory_path_str}", file=sys.stderr)
        return []

    # Use rglob to recursively find all files matching the pattern '*.sif'
    print(f"Searching for .sif files in: {directory_path}...")
    found_paths = list(directory_path.rglob('*.sif')) # Collect paths first to get count
    print(f"Found {len(found_paths)} potential .sif files.")

    for file_path in found_paths:
        # file_path from rglob will be absolute here since directory_path is resolved
        if file_path.is_file():
            # Calculate path relative to the original search directory
            try:
                relative_path = file_path.relative_to(directory_path)
                # Convert the Path object to a string for the final list
                sif_files_relative.append(str(relative_path))
            except ValueError:
                # Should not happen if rglob starts within directory_path, but handle defensively
                print(f"Warning: Could not make path relative: {file_path}", file=sys.stderr)


    return sif_files_relative

def append_result_to_csv(result: ProcessingResult, filename: str):
    """Appends a single ProcessingResult to the specified CSV file."""
    # This function doesn't need changes, as result.filepath is now relative
    file_exists = Path(filename).exists()
    is_empty = not file_exists or os.path.getsize(filename) == 0
    try:
        with open(filename, mode='a', newline='', encoding='utf-8') as csvfile:
            fieldnames = ['filepath', 'status', 'return_code', 'duration_seconds']
            writer = csv.writer(csvfile)
            if is_empty:
                writer.writerow(fieldnames)
            writer.writerow([
                result.filepath, # This is now the relative path
                result.status,
                result.return_code if result.return_code is not None else '',
                result.duration_seconds
            ])
    except IOError as e:
        print(f"\nError writing to CSV file {filename}: {e}", file=sys.stderr)
    except Exception as e:
        print(f"\nUnexpected error writing to CSV {filename}: {e}", file=sys.stderr)

def read_processed_files(filename: str) -> Dict[str, float]:
    """
    Reads the CSV, returning a dictionary mapping processed relative filepaths
    to their previously recorded durations.
    """
    # This function doesn't need changes, it reads whatever path is in the CSV
    processed_data: Dict[str, float] = {}
    required_headers = ['filepath', 'duration_seconds']

    if not Path(filename).exists():
        print(f"Results file '{filename}' not found. Starting fresh.")
        return processed_data

    try:
        with open(filename, mode='r', newline='', encoding='utf-8') as csvfile:
            reader = csv.DictReader(csvfile)
            if not all(header in reader.fieldnames for header in required_headers):
                 print(f"Warning: CSV file '{filename}' is missing required headers ('filepath', 'duration_seconds'). Cannot determine previously processed files accurately.", file=sys.stderr)
                 return processed_data

            for row_num, row in enumerate(reader, start=2):
                filepath = row.get('filepath') # This will be the relative path from the CSV
                duration_str = row.get('duration_seconds')
                duration_float = 0.0

                if filepath:
                    if duration_str:
                        try:
                            duration_float = float(duration_str)
                        except ValueError:
                            print(f"Warning: Could not parse duration '{duration_str}' for relative path '{filepath}' in {filename} (row {row_num}). Storing duration as 0.0.", file=sys.stderr)
                    else:
                         print(f"Warning: Missing duration for relative path '{filepath}' in {filename} (row {row_num}). Storing duration as 0.0.", file=sys.stderr)
                    processed_data[filepath] = duration_float

        print(f"Read {len(processed_data)} previously processed relative file path entries from '{filename}'.")

    except FileNotFoundError:
         print(f"Results file '{filename}' not found during read attempt. Starting fresh.")
    except Exception as e:
        print(f"\nError reading CSV file {filename}: {e}. Skipping previously processed files based on incomplete data.", file=sys.stderr)
        return processed_data

    return processed_data


def main():
    """
    Main function to handle command-line arguments, find .sif files (relative paths),
    run an executable on each found file (timed), suppressing the executable's output,
    collecting results, logging results incrementally to CSV, skipping already processed files
    (showing their previous time), and calculating cumulative processing time.
    """
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <executable_path> <search_directory>", file=sys.stderr)
        sys.exit(1)

    executable_path_str = sys.argv[1]
    target_directory_str = sys.argv[2] # Keep original string for reconstructing absolute paths

    # --- Validate Executable Path ---
    exe_path = Path(executable_path_str)
    # (Validation checks remain the same)...
    if not exe_path.exists():
        print(f"Error: Executable not found: {executable_path_str}", file=sys.stderr)
        sys.exit(1)
    if not exe_path.is_file():
        print(f"Error: Executable path is not a file: {executable_path_str}", file=sys.stderr)
        sys.exit(1)
    if not os.access(str(exe_path), os.X_OK):
        print(f"Error: Provided path is not executable: {executable_path_str}", file=sys.stderr)
        sys.exit(1)
    executable_abs_path = str(exe_path.resolve()) # Resolve executable path once

    # --- Read previously processed files data (relative filepath -> duration) ---
    already_processed_data = read_processed_files(CSV_FILENAME)
    previous_total_duration = sum(already_processed_data.values())
    if len(already_processed_data) > 0:
        print(f"Sum of previously recorded durations: {previous_total_duration:.2f} s")


    # --- Find the .sif files (relative paths) ---
    found_files = find_sif_files_recursively(target_directory_str) # Now returns relative paths

    # --- Execute the command for each found file and collect results ---
    results_list_this_run: List[ProcessingResult] = [] # Results from *this run*
    skipped_count = 0 # Initialize skipped counter

    if found_files:
        total_files_count = len(found_files) # Get total count for formatting
        width = len(str(total_files_count))
        # Resolve search directory path for reconstructing absolute paths later
        base_search_path = Path(target_directory_str).resolve()

        print(f"\nProcessing {total_files_count} .sif files using '{executable_abs_path}' (output suppressed)...")
        print(f"Results will be logged incrementally to: {CSV_FILENAME}")

        # Use enumerate starting from 1 for the counter
        for idx, sif_relative_path in enumerate(found_files, start=1): # Renamed variable
            # Format the counter string with zero padding
            counter_str = f"{idx:0{width}}/{total_files_count}"

            # --- Check if file was already processed (using relative path) ---
            if sif_relative_path in already_processed_data:
                previous_duration = already_processed_data[sif_relative_path]
                # Updated Skipping message to include previous duration
                print(f"---> [{counter_str}] Skipping (took {previous_duration:.1f} s): {sif_relative_path}")
                skipped_count += 1
                continue # Move to the next file

            # --- Process the file ---
            # Reconstruct absolute path for the subprocess
            sif_absolute_path = str(base_search_path / sif_relative_path)

            # Initialize variables for this iteration's result
            status: str = "Unknown Error"
            return_code: Optional[int] = None
            duration: float = 0.0
            result_obj: Optional[ProcessingResult] = None # Define here for broader scope

            # Print message before starting, including the counter (using relative path)
            print(f"---> [{counter_str}] Processing: {sif_relative_path}", end="")
            sys.stdout.flush() # Flush output buffer
            start_time = time.perf_counter() # Start timer for this file

            try:
                # Run the command, using the *absolute* path for the .sif file
                result = subprocess.run(
                    [executable_abs_path, sif_absolute_path], # Use absolute path here
                    check=False,
                    stdout=subprocess.DEVNULL, # Suppress standard output
                    stderr=subprocess.DEVNULL  # Suppress standard error
                )
                duration = time.perf_counter() - start_time
                return_code = result.returncode

                if result.returncode == 0:
                    status = "Success"
                    print(f" [{status}] ({duration:.1f} s)")
                else:
                    status = "Failed"
                    print(f" [{status} - Code: {return_code}] ({duration:.1f} s)")

                # Store the RELATIVE path in the result object
                result_obj = ProcessingResult(sif_relative_path, status, return_code, duration)

            except OSError as e:
                 duration = time.perf_counter() - start_time
                 status = "OS Error"
                 print(f" [OS Error: {e}] ({duration:.1f} s)")
                 # Store the RELATIVE path in the result object
                 result_obj = ProcessingResult(sif_relative_path, status, None, duration)
            except Exception as e:
                 duration = time.perf_counter() - start_time
                 status = "Unexpected Error"
                 print(f" [{status}: {e}] ({duration:.1f} s)")
                 # Store the RELATIVE path in the result object
                 result_obj = ProcessingResult(sif_relative_path, status, None, duration)

            # Append the result object to the list AND write to CSV
            if result_obj: # Ensure result_obj was created
                 results_list_this_run.append(result_obj)
                 append_result_to_csv(result_obj, CSV_FILENAME) # Log result immediately

        # --- Calculate Durations ---
        current_run_duration = sum(r.duration_seconds for r in results_list_this_run)
        # Recalculate cumulative duration using the sum derived earlier
        cumulative_total_duration = previous_total_duration + current_run_duration

        # --- Generate Summary from collected results ---
        print("\n--- Summary ---")
        total_files_processed_this_run = len(results_list_this_run)
        # Calculate counts based on the results *from this run*
        success_count = sum(1 for r in results_list_this_run if r.status == "Success")
        fail_count = total_files_processed_this_run - success_count # All non-Success states are failures

        print(f"Total .sif files found: {total_files_count}")
        print(f"Skipped (previously processed): {skipped_count}")
        print(f"Attempted processing this run: {total_files_processed_this_run}")
        print(f"Succeeded this run (exit code 0): {success_count}")
        print(f"Failed this run (non-zero exit or error): {fail_count}")
        print(f"Total processing time this run: {current_run_duration:.2f} s") # Clarified label
        print(f"Cumulative total processing time (from CSV + this run): {cumulative_total_duration:.2f} s") # Added cumulative time
        print(f"Results logged to: {CSV_FILENAME}")

    else:
        # Only print if the find function didn't already print an error
        dir_path_obj = Path(target_directory_str) # Check original string path
        if dir_path_obj.exists() and dir_path_obj.is_dir():
             print("\nNo .sif files found in the specified directory.")
        # Error messages for invalid directory are handled in find_sif_files_recursively

# Standard Python entry point guard
if __name__ == "__main__":
    main()

