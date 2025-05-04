#!/usr/bin/env python3

import sys
from pathlib import Path
import subprocess
import time
from dataclasses import dataclass, asdict, field
from typing import List, Optional, Tuple, Dict
import json
import argparse


@dataclass
class TestMetaData:
    """Stores metadata about the test run."""
    num_passes: int


@dataclass
class ProcessingResult:
    """Holds the result of processing a single .sif file across multiple passes."""
    id_num: int
    filepath: str
    status: List[str] = field(default_factory=list)
    return_code: List[Optional[int]] = field(default_factory=list)
    duration_nanoseconds: List[int] = field(default_factory=list)


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


def read_results_json(filename: str, requested_num_passes: int) -> Tuple[Optional[TestMetaData], Dict[str, List[int]], List[Dict]]:
    """
    Reads the JSON results file, validates pass count, and extracts data.

    Returns:
        A tuple containing:
        - TestMetaData object (or None if file invalid/missing metadata).
        - Dict mapping filepath to list of durations (ns) for skipping.
        - List of all previous result dictionaries.
    """
    processed_data_for_skipping = {}
    previous_results_list = []
    metadata = None
    validated_num_passes = requested_num_passes

    if not Path(filename).exists():
        print(f"Results file '{filename}' not found. Starting fresh.")
        metadata = TestMetaData(num_passes=validated_num_passes)
        return metadata, processed_data_for_skipping, previous_results_list

    print(f"Reading previously processed files from '{filename}'...")
    with open(filename, mode='r', encoding='utf-8') as f:
        content = f.read()
        if not content.strip():
            print(f"Results file '{filename}' is empty. Starting fresh.")
            metadata = TestMetaData(num_passes=validated_num_passes)
            return metadata, processed_data_for_skipping, previous_results_list

        data = json.loads(content)

        metadata_dict = data.get('metadata')
        if isinstance(metadata_dict, dict):
            file_num_passes = metadata_dict.get('num_passes')
            if isinstance(file_num_passes, int):
                # Check consistency
                if requested_num_passes != file_num_passes:
                    print(f"Error: Command line requested --num-passes={requested_num_passes}, but results file '{filename}' indicates {file_num_passes} passes were used previously.", file=sys.stderr)
                    print('Please use a different JSON file or ensure --num-passes matches the existing file.', file=sys.stderr)
                    sys.exit(1)

                validated_num_passes = file_num_passes
                metadata = TestMetaData(num_passes=validated_num_passes)
                print(f'Using existing number of passes from file: {validated_num_passes}')
        else:
            print(f"Warning: Metadata missing or invalid in '{filename}'. Using requested value: {validated_num_passes}", file=sys.stderr)
            metadata = TestMetaData(num_passes=validated_num_passes)

        previous_results_list = data.get('results', [])
        if not isinstance(previous_results_list, list):
            print(f"Warning: Expected 'results' to be a list in JSON file '{filename}', found {type(previous_results_list)}. Ignoring previous results.", file=sys.stderr)
            previous_results_list = [] # Reset 

        # Populate the dictionary for skipping logic
        valid_entry_count = 0
        total_previous_duration_ns = 0
        for idx, result_dict in enumerate(previous_results_list):
            if isinstance(result_dict, dict):
                filepath = result_dict.get('filepath')
                duration_list = result_dict.get('duration_nanoseconds')

                if (filepath is not None) and isinstance(duration_list, list):
                    valid_durations_ns = []
                    for dur_val in duration_list:
                        dur_int = int(dur_val)
                        valid_durations_ns.append(dur_int)
                        total_previous_duration_ns += dur_int
                    processed_data_for_skipping[filepath] = valid_durations_ns
                    valid_entry_count += 1

    print(f"Read {len(previous_results_list)} previous result entries from '{filename}'.")
    print(f'Found {len(processed_data_for_skipping)} entries with valid paths for skipping logic.')
    total_previous_duration_s = total_previous_duration_ns / 1_000_000_000.0
    print(f'Sum of previously recorded durations: {total_previous_duration_s:.2f} s')

    return metadata, processed_data_for_skipping, previous_results_list


def write_results_to_json(metadata: TestMetaData, all_results_as_dicts: List[Dict], filename: str):
    """Writes the metadata and entire list of result dictionaries to a JSON file."""
    output_data = {
        'metadata': asdict(metadata),
        'results': all_results_as_dicts
    }
    with open(filename, mode='w', encoding='utf-8') as f:
        json.dump(output_data, f, indent=4)


def main():
    """
    Main function implementing multi-pass execution.
    """
    parser = argparse.ArgumentParser(
        description='Finds .sif files recursively and runs an executable on them for multiple passes, logging results to JSON, applying a timeout, and skipping previously processed files.'
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
        '--json',
        default='results.json',
        help='Filename for reading/writing processing results in JSON format (default: results.json)'
    )
    parser.add_argument(
        '--num-passes',
        type=int,
        default=5,
        help='Number of times to run the executable on each .sif file (default: 5)'
    )
    parser.add_argument(
        '--timeout-limit',
        type=int,
        default=900, # Default timeout 15 minutes
        help='Timeout limit in seconds for each subprocess run (0 to disable, default: 900)'
    )

    # Retrieve arguments
    args = parser.parse_args()
    executable_path_str = args.executable_path
    target_directory_str = args.search_directory
    json_filename = args.json
    requested_num_passes = args.num_passes
    timeout_limit = args.timeout_limit

    if requested_num_passes < 1:
         print('Error: --num-passes must be 1 or greater.', file=sys.stderr)
         sys.exit(1)

    exe_path = Path(executable_path_str)
    executable_abs_path = str(exe_path.resolve())

    if timeout_limit <= 0:
        print('Timeout limit disabled.')
        effective_timeout = None
    else:
        print(f'Timeout limit set to {timeout_limit} seconds.')
        effective_timeout = timeout_limit

    # Read previously processed files data
    metadata, already_processed_data, previous_results_list_dicts = read_results_json(json_filename, requested_num_passes)
    num_passes = metadata.num_passes # Use the validated number of passes
    print(f'Target number of passes per file: {num_passes}')

    found_files = find_sif_files_recursively(target_directory_str)

    # Create a dictionary from the previous results list for easier lookup/update
    all_results_map = {}
    for result_dict in previous_results_list_dicts:
        filepath = result_dict.get('filepath')
        all_results_map[filepath] = ProcessingResult(
            id_num=result_dict.get('id_num', -1),
            filepath=filepath,
            status=result_dict.get('status', []),
            return_code=result_dict.get('return_code', []),
            duration_nanoseconds=result_dict.get('duration_nanoseconds', []),
        )

    results_this_run_count = 0
    skipped_count = 0
    timed_out_passes = 0

    if found_files:
        total_files_count = len(found_files)
        width = len(str(total_files_count))
        base_search_path = Path(target_directory_str).resolve()

        print(f"\nProcessing {total_files_count} .sif files using '{executable_abs_path}' (output suppressed)...")
        print(f'Results will be logged to: {json_filename}')

        for idx, sif_relative_path in enumerate(found_files, start=1):
            counter_str = f'{idx:0{width}}'
            sif_basename = Path(sif_relative_path).name

            # Get or create the result object for this file
            result_obj = all_results_map.get(sif_relative_path)
            if result_obj is None:
                result_obj = ProcessingResult(id_num=idx, filepath=sif_relative_path)
                all_results_map[sif_relative_path] = result_obj

            # Determine passes needed
            passes_done = len(result_obj.duration_nanoseconds)
            passes_needed = num_passes - passes_done

            if passes_needed <= 0:
                # Format existing durations for skip message
                durations_str = ', '.join([f'{(ns / 1_000_000_000.0):.1f}s' for ns in result_obj.duration_nanoseconds])
                print(f'  {counter_str} [Skip] {sif_basename} -- {durations_str}')
                skipped_count += 1
                continue
            else:
                results_this_run_count += 1
                print(f'  {counter_str} [Running] {sif_basename} ({passes_done}/{num_passes} passes done, {passes_needed} needed)')
                sys.stdout.flush()

            sif_absolute_path = str(base_search_path / sif_relative_path)
            command = [executable_abs_path, sif_absolute_path]

            for pass_num in range(passes_needed):
                status = 'Unknown Error'
                return_code = None
                duration_ns = 0

                start_time_ns = time.perf_counter_ns()
                try:
                    sub_result = subprocess.run(
                        command, check=False, stdout=subprocess.DEVNULL,
                        stderr=subprocess.DEVNULL, timeout=effective_timeout
                    )
                    duration_ns = time.perf_counter_ns() - start_time_ns
                    return_code = sub_result.returncode
                    if sub_result.returncode == 0:
                        status = 'Success'
                        print(f'    ({status}) -- {duration_ns / 1_000_000_000.0:.1f} s')
                    else:
                        status = 'Failed'
                        print(f'    ({status}: {return_code}) -- {duration_ns / 1_000_000_000.0:.1f} s')

                except subprocess.TimeoutExpired:
                    duration_ns = time.perf_counter_ns() - start_time_ns
                    status = 'TimedOut'
                    return_code = 1
                    timed_out_passes += 1
                    print(f'    ({status}) -- {duration_ns / 1_000_000_000.0:.1f} s')

                except OSError as e:
                    duration_ns = time.perf_counter_ns() - start_time_ns
                    status = 'OS Error'
                    print(f'    ({status}: {e}) -- {duration_ns / 1_000_000_000.0:.1f} s')

                except Exception as e:
                    duration_ns = time.perf_counter_ns() - start_time_ns
                    status = 'Unexpected Error'
                    print(f'    ({status}: {e}) -- {duration_ns / 1_000_000_000.0:.1f} s')

                # Append results for this pass to the object's lists
                result_obj.status.append(status)
                result_obj.return_code.append(return_code)
                result_obj.duration_nanoseconds.append(duration_ns)

                # Rewrite JSON after each pass
                # Convert all current result objects back to dictionaries
                all_results_dicts_updated = [asdict(r) for r in all_results_map.values()]
                all_results_dicts_updated.sort(key=lambda x: x.get('id_num', float('inf')))
                write_results_to_json(metadata, all_results_dicts_updated, json_filename)

        print('\n--- Summary ---')
        final_results_list = list(all_results_map.values())
        succeeded_passes = sum(s == 'Success' for r in final_results_list for s in r.status)
        failed_passes = sum(s == 'Failed' for r in final_results_list for s in r.status)
        os_error_passes = sum(s == 'OS Error' for r in final_results_list for s in r.status)
        unexpected_error_passes = sum(s == 'Unexpected Error' for r in final_results_list for s in r.status)

        # Calculate cumulative duration from the final map
        cumulative_total_duration_ns = sum(d for r in final_results_list for d in r.duration_nanoseconds)
        cumulative_total_duration_s = cumulative_total_duration_ns / 1_000_000_000.0

        print(f'Total .sif files found: {total_files_count}')
        print(f'Skipped (already fully processed): {skipped_count}')
        print(f'Files processed (at least partially) this run: {results_this_run_count}')
        print(f'Total passes required: {total_files_count * num_passes}')
        print(f'Total passes completed (all runs): {succeeded_passes + failed_passes + timed_out_passes + os_error_passes + unexpected_error_passes}')
        print(f'  Succeeded passes: {succeeded_passes}')
        print(f'  Failed passes: {failed_passes}')
        print(f'  Timed Out passes: {timed_out_passes}')
        print(f'  OS Error passes: {os_error_passes}')
        print(f'  Unexpected Error passes: {unexpected_error_passes}')
        print(f'Cumulative processing time (all passes): {cumulative_total_duration_s:.2f} s')
        print(f'Results logged to: {json_filename}')
    else:
        dir_path_obj = Path(target_directory_str)
        if dir_path_obj.exists() and dir_path_obj.is_dir():
             print('\nNo .sif files found in the specified directory.')


if __name__ == '__main__':
    main()
