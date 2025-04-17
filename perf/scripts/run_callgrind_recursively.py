#!/usr/bin/env python3

import sys
from pathlib import Path
import os
import subprocess
import time
from dataclasses import dataclass
from typing import List, Optional, Dict
import csv
import argparse
import collections


@dataclass
class ProcessingResult:
    """Holds the result of processing a single .sif file."""
    id_num: int
    filepath: str
    status: str                             # e.g. "Success", "Failed", "TimedOut", "OS Error", "Unexpected Error"
    return_code: Optional[int]
    duration_seconds: float
    callgrind_output: Optional[str] = None  # Path to callgrind output file


def find_sif_files_recursively(directory_path_str: str) -> list[str]:
    """
    Recursively finds all files ending with '.sif' in the given directory,
    sorted alphabetically by relative path.

    Args:
        directory_path_str: The path to the directory to search.

    Returns:
        A sorted list of paths relative to the search directory for the found '.sif' files.
        Returns an empty list if the directory is invalid or not found.
    """
    sif_files_relative = []
    directory_path = Path(directory_path_str).resolve(strict=True)

    # Use rglob to recursively find all files matching the pattern '*.sif'
    print(f'Searching for .sif files in: {directory_path}...')
    found_paths = list(directory_path.rglob('*.sif'))

    print(f'Found {len(found_paths)} potential .sif files.')
    for file_path in found_paths:
        relative_path = file_path.relative_to(directory_path)
        sif_files_relative.append(str(relative_path))

    # Sort the list of relative paths (alphabetically)
    sif_files_relative.sort()

    return sif_files_relative


def append_result_to_csv(result: ProcessingResult, filename: str):
    """Appends a single ProcessingResult to the specified CSV file."""

    fieldnames = ['id_num', 'filepath', 'status', 'return_code', 'duration_seconds', 'callgrind_output']

    file_exists = Path(filename).exists()
    is_empty = not file_exists or os.path.getsize(filename) == 0

    with open(filename, mode='a', newline='', encoding='utf-8') as csv_file:
        writer = csv.writer(csv_file)

        # Brand new?
        if is_empty:
            writer.writerow(fieldnames)

        writer.writerow([
            result.id_num,
            result.filepath,
            result.status,
            result.return_code if result.return_code is not None else '',
            result.duration_seconds,
            result.callgrind_output if result.callgrind_output is not None else ''
        ])


def read_processed_files(filename: str) -> Dict[str, float]:
    """
    Reads the CSV, returning a dictionary mapping processed relative filepaths
    to their previously recorded durations.
    """
    processed_data: Dict[str, float] = {}
    required_headers = ['filepath', 'duration_seconds']

    if not Path(filename).exists():
        print(f"Results file '{filename}' not found. Starting fresh.")
        return processed_data

    with open(filename, mode='r', newline='', encoding='utf-8') as csvfile:
        reader = csv.DictReader(csvfile)
        if not all(header in reader.fieldnames for header in required_headers):
             print(f"Warning: CSV file '{filename}' is missing required headers ('filepath', 'duration_seconds'). Cannot determine previously processed files accurately.", file=sys.stderr)
             # Try reading just filepaths if possible for skipping
             if 'filepath' in reader.fieldnames:
                 for row in reader:
                     if row.get('filepath'):
                         processed_data[row['filepath']] = 0.0
             return processed_data

        for row_num, row in enumerate(reader, start=2):
            filepath = row.get('filepath') # This will be the relative path from the CSV
            duration_str = row.get('duration_seconds')
            duration_float = 0.0

            if filepath:
                if duration_str:
                    duration_float = float(duration_str)
                processed_data[filepath] = duration_float

    print(f"Read {len(processed_data)} previously processed relative file path entries from '{filename}'.")

    return processed_data


def main():
    """
    Main function using argparse for command-line arguments, supporting optional Callgrind and timeout.
    """
    parser = argparse.ArgumentParser(
        description='Finds .sif files recursively and runs an executable on them (optionally under callgrind), logging results, applying a timeout, and skipping previously processed files.'
    )
    parser.add_argument(
        'executable_path',
        help='Path to the executable to run on each .sif file.'
    )
    parser.add_argument(
        'search_directory',
        help='Directory to search recursively for .sif files.'
    )
    parser.add_argument(
        '--csv',
        default='callgrind_results.csv',
        help='Filename for reading/writing processing results (default: callgrind_results.csv)'
    )
    parser.add_argument(
        '--callgrind-output-dir',
        default='callgrind_output',
        help='Directory to save callgrind output files (default: callgrind_output)'
    )
    parser.add_argument(
        '--timeout-limit',
        type=int,
        default=900,
        help='Timeout limit in seconds for each subprocess run (0 to disable, default: 900)'
    )
    args = parser.parse_args()

    executable_path_str = args.executable_path
    target_directory_str = args.search_directory
    csv_filename = args.csv
    callgrind_output_dir = args.callgrind_output_dir
    timeout_limit = args.timeout_limit

    exe_path = Path(executable_path_str)
    executable_abs_path = str(exe_path.resolve())

    # Display timeout setting
    if timeout_limit <= 0:
        print('Timeout limit disabled.')
        effective_timeout = None
    else:
        print(f'Timeout limit set to {timeout_limit} seconds.')
        effective_timeout = timeout_limit

    # Create Callgrind Output Directory
    callgrind_dir_path = Path(callgrind_output_dir)
    callgrind_dir_path.mkdir(parents=True, exist_ok=True)
    print(f'Callgrind output will be saved to: {callgrind_dir_path.resolve()}')

    # Read previously processed files
    already_processed_data = read_processed_files(csv_filename)
    previous_total_duration = sum(already_processed_data.values())
    if len(already_processed_data) > 0:
        print(f'Sum of previously recorded durations: {previous_total_duration:.2f} s')

    # Get files
    found_files = find_sif_files_recursively(target_directory_str)

    # check for unique base names
    basename_counts = collections.Counter()
    if found_files:
        basename_counts = collections.Counter(Path(f).name for f in found_files)

    # Execute the command for each .sif file
    results_list_this_run = []
    skipped_count = 0
    timed_out_count = 0

    if found_files:
        total_files_count = len(found_files)
        width = len(str(total_files_count))
        base_search_path = Path(target_directory_str).resolve()

        print(f"\nProcessing {total_files_count} .sif files using Valgrind/Callgrind on '{executable_abs_path}' (output suppressed)...")
        print(f'Results will be logged incrementally to: {csv_filename}')

        # Use enumerate starting from 1 for the counter
        for idx, sif_relative_path in enumerate(found_files, start=1):
            counter_str = f'{idx:0{width}}'
            sif_basename = Path(sif_relative_path).name

            # Check if we've already processed this file
            if sif_relative_path in already_processed_data:
                previous_duration = already_processed_data[sif_relative_path]
                print(f'  {counter_str} [Skip] {sif_basename} -- {previous_duration:.1f} s')
                skipped_count += 1
            else:
                sif_absolute_path = str(base_search_path / sif_relative_path)
                callgrind_out_relative_path = None

                # build the output file name
                if basename_counts[sif_basename] > 1:
                    callgrind_out_name = f'{counter_str}.{sif_basename}.callgrind'
                else:
                    callgrind_out_name = f'{sif_basename}.callgrind'

                callgrind_out_path = callgrind_dir_path / callgrind_out_name
                callgrind_out_relative_path = str(callgrind_out_path) # Relative to CWD

                # Build the command to run
                command = [
                    'valgrind',
                    '--tool=callgrind',
                    f'--callgrind-out-file={callgrind_out_relative_path}',
                    executable_abs_path,
                    sif_absolute_path
                ]

                # Initialize variables for this iteration's result
                status = 'Unknown Error'
                return_code = None
                duration = 0.0
                result_obj = None

                # Print message before starting
                print(f'  {counter_str} [Running] {sif_basename}', end='')
                sys.stdout.flush()
                start_time = time.perf_counter()

                try:
                    result = subprocess.run(
                        command,
                        check=False,
                        stdout=subprocess.DEVNULL,
                        stderr=subprocess.DEVNULL,
                        timeout=effective_timeout
                    )
                    duration = time.perf_counter() - start_time
                    return_code = result.returncode

                    if result.returncode == 0:
                        status = 'Success'
                        print(f' ({status}) -- {duration:.1f} s')
                    else:
                        status = 'Failed'
                        print(f' ({status}: {return_code}) -- {duration:.1f} s')

                    result_obj = ProcessingResult(idx, sif_relative_path, status, return_code, duration, callgrind_out_relative_path)

                except subprocess.TimeoutExpired:
                    # Handle timeout specifically
                    duration = time.perf_counter() - start_time
                    status = 'TimedOut'
                    return_code = 1
                    timed_out_count += 1
                    print(f' ({status}) -- {duration:.1f} s')
                    result_obj = ProcessingResult(idx, sif_relative_path, status, return_code, duration, callgrind_out_relative_path)

                except OSError as e:
                    duration = time.perf_counter() - start_time
                    status = 'OS Error'
                    print(f' ({status}: {e}) -- {duration:.1f} s')
                    result_obj = ProcessingResult(idx, sif_relative_path, status, None, duration, None)

                except Exception as e:
                    # Other unexpected errors
                    duration = time.perf_counter() - start_time
                    status = 'Unexpected Error'
                    print(f' ({status}: {e}) -- {duration:.1f} s')
                    result_obj = ProcessingResult(idx, sif_relative_path, status, None, duration, None)

                # Append the result object to the list AND write to CSV
                if result_obj:
                    results_list_this_run.append(result_obj)
                    append_result_to_csv(result_obj, csv_filename)

        current_run_duration = sum(r.duration_seconds for r in results_list_this_run)
        cumulative_total_duration = previous_total_duration + current_run_duration

        print('\n--- Summary ---')
        total_files_processed_this_run = len(results_list_this_run)
        success_count = sum(1 for r in results_list_this_run if r.status == 'Success')
        fail_count = sum(1 for r in results_list_this_run if r.status not in ['Success', 'TimedOut'])

        print(f'Total .sif files found: {total_files_count}')
        print(f'Skipped (previously processed): {skipped_count}')
        print(f'Attempted processing this run: {total_files_processed_this_run}')
        print(f'Succeeded this run (exit code 0): {success_count}')
        print(f'Timed Out this run: {timed_out_count}')
        print(f'Failed this run (non-zero exit or error): {fail_count}')
        print(f'Total processing time this run: {current_run_duration:.2f} s')
        print(f'Cumulative total processing time (from CSV + this run): {cumulative_total_duration:.2f} s')
        print(f'Results logged to: {csv_filename}')
        print(f'Callgrind output saved to directory: {callgrind_output_dir}')
    else:
        dir_path_obj = Path(target_directory_str)
        if dir_path_obj.exists() and dir_path_obj.is_dir():
            print('\nNo .sif files found in the specified directory.')


if __name__ == '__main__':
    main()

