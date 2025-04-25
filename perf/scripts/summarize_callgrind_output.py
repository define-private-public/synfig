#!/usr/bin/env python3

import os
import sys
import subprocess
import argparse
import shutil
from dataclasses import dataclass
from collections import defaultdict


@dataclass
class CallgrindEntry:
    """
    Represents a single entry from the callgrind_annotate output.
    """
    instructions: int
    percent: float
    function: str


def parse_callgrind_output(text):
    """
    Parses the output of callgrind_annotate to extract instruction counts,
    percentages, and function names using a precise character-by-character
    parsing algorithm.

    Args:
        text (str): The text output from callgrind_annotate.

    Returns:
        list: A list of CallgrindEntry objects.  Returns an empty list
              on error or if no data is found.
    """
    results = []
    lines = text.strip().split('\n')
    in_function_section = False

    for line in lines:
        if 'file:function' in line:
            in_function_section = True
            continue  # Skip the header line
        elif 'PROGRAM TOTALS' in line:
            in_function_section = False
            continue
        elif line.startswith('-' * 10):  # Ignore lines of dashes
            continue

        if in_function_section:
            line = line.strip()
            instructions_str = ''
            percent_str = ''
            function_name = ''
            parsing_instructions = True
            parsing_percent = False
            parsing_function = False

            for i, char in enumerate(line):
                if parsing_instructions:
                    if char.isdigit() or char == ',':
                        instructions_str += char
                    elif char == ' ':
                        parsing_instructions = False
                        parsing_percent = True
                elif parsing_percent:
                    if char == '(':
                        continue  # Skip the opening parenthesis
                    elif char.isdigit() or char == '.':
                        percent_str += char
                    elif char == ')':
                        parsing_percent = False
                        parsing_function = True
                    elif char == ' ':
                        pass #ignore the space
                elif parsing_function:
                    function_name += char

            try:
                instructions = int(instructions_str.replace(',', ''))
                percent = float(percent_str)
                function = function_name.strip()
                results.append(CallgrindEntry(instructions, percent, function))
            except ValueError as e:
                print(f'Error: Could not convert instruction count or percentage to number: {line} - {e}')
                return []

    return results


def process_callgrind_files(directory, purge_cache=False, limit=250):
    """
    Recursively searches the given directory for files ending in '.callgrind',
    sorts them, processes them with callgrind_annotate, and parses the output.
    Uses a cache to store the output of callgrind_annotate.

    Args:
        directory (str): The directory to search.
        purge_cache (bool): If True, clears the annotation cache directory.
        limit (int): Maximum number of results to print.  0 for all.
    """
    callgrind_files = []
    for root, _, files in os.walk(directory):
        for filename in files:
            if filename.endswith('.callgrind'):
                filepath = os.path.join(root, filename)
                callgrind_files.append(filepath)

    callgrind_files.sort()

    if not callgrind_files:
        print(f"No .callgrind files found in '{directory}' or its subdirectories.")
        return

    # Create the annotation cache directory
    cache_dir = os.path.join(directory, 'annotation_cache')
    if purge_cache:
        print('Purging annotation cache...')
        shutil.rmtree(cache_dir)
    os.makedirs(cache_dir, exist_ok=True)

    combined_results = defaultdict(int)
    combined_percents = defaultdict(list)
    print(f'Processing {len(callgrind_files)} .callgrind files:')

    for i, filepath in enumerate(callgrind_files):
        basename = os.path.basename(filepath)
        cache_file = os.path.join(cache_dir, basename + '_annotation')
        print(basename)

        if os.path.exists(cache_file):
            # Load from cache
            try:
                with open(cache_file, 'r') as infile:
                    annotate_output = infile.read()
            except Exception as e:
                print(f'Error reading cache file {cache_file}: {e}')
                continue
        else:
            # Check if the .callgrind file is empty
            if os.path.getsize(filepath) == 0:
                print(f'  Warning: {basename} is empty. Skipping.')
                continue

            # Run callgrind_annotate (and save to cache)
            try:
                command = ['callgrind_annotate', '--auto=no', filepath]
                process = subprocess.run(command, capture_output=True, text=True, check=True)
                annotate_output = process.stdout
                with open(cache_file, 'w') as outfile:
                    outfile.write(annotate_output)
            except subprocess.CalledProcessError as e:
                print(f'Error running callgrind_annotate on {basename}:')
                print(e.stderr)
                continue
            except Exception as e:
                print(f'An unexpected error occurred while processing {basename}: {e}')
                continue

        # Parse the output
        parsed_entries = parse_callgrind_output(annotate_output)
        if parsed_entries:
            for entry in parsed_entries:
                combined_results[entry.function] += entry.instructions
                combined_percents[entry.function].append(entry.percent)
        else:
            print(f'Error: Failed to parse callgrind_annotate output for {basename}. Skipping file.')

    # Print the combined results, sorted by instruction count
    sorted_results = sorted(combined_results.items(), key=lambda item: item[1], reverse=True)
    print('\n--- Combined Results (Sorted by Total Instructions) ---')

    for i, (function_name, total_instructions) in enumerate(sorted_results):
        if (limit > 0) and (i >= limit):
            break # Stop printing after limit

        rank = i + 1
        percentages = combined_percents.get(function_name, [])
        avg_percent = (sum(percentages) / len(callgrind_files)) if percentages else 0.0

        print(f'{rank:03} ({avg_percent:.1f}%): {total_instructions} -- {function_name}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='Recursively finds .callgrind files, runs callgrind_annotate on them, '
                    'and summarizes the combined instruction counts for each function.'
    )
    parser.add_argument(
        'directory',
        help='The directory to search recursively for .callgrind files.'
    )
    parser.add_argument(
        '--purge-cache',
        action='store_true',
        help='If set, clears the annotation cache directory before processing.'
    )
    parser.add_argument(
        '--limit',
        type=int,
        default=250,
        help='Maximum number of results to print. Use 0 for all results. (default: 250)'
    )
    args = parser.parse_args()

    if args.limit < 0:
        parser.error('argument --limit: must be a non-negative integer.')

    if not os.path.isdir(args.directory):
        parser.error(f"argument directory: '{args.directory}' is not a valid directory.")

    process_callgrind_files(args.directory, args.purge_cache, args.limit)
