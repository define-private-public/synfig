#!/usr/bin/env python3

import argparse
import json
import csv
from typing import List, Dict, Optional, Tuple
import statistics


def load_json(filename: str) -> Optional[Tuple[int, List[Dict]]]:
    """
    Loads the JSON results file and pulls some data

    Args:
        filename: The path to the input JSON file.

    Returns:
        A tuple containing (num_passes, results_list) if validation passes,
        otherwise None.
    """

    with open(filename, mode='r', encoding='utf-8') as f:
        content = f.read()
        data = json.loads(content)

    metadata_dict = data.get('metadata')
    num_passes = metadata_dict.get('num_passes')
    results_list = data.get('results')

    return num_passes, results_list


def main():
    """
    Main function to parse arguments, load/validate JSON, calculate stats,
    write CSV file, and print summary stats to stdout.
    """
    parser = argparse.ArgumentParser(
        description='Reads a results JSON file, validates it, calculates duration stats, writes selected fields to a CSV file, and prints summary stats to stdout.'
    )
    parser.add_argument(
        'json_file',
        help='Path to the input JSON results file.'
    )
    parser.add_argument(
        '--csv',
        default='results.csv',
        help='Filename for writing the output CSV data (default: results.csv)'
    )
    args = parser.parse_args()
    output_csv_filename = args.csv

    # Load the JSON data
    num_passes, results_list = load_json(args.json_file)

    # Initialize variables for summary stats
    total_cumulative_ns = 0
    total_mean_ns = 0
    total_fastest_ns = 0

    # Open the output CSV file for writing
    with open(output_csv_filename, 'w', newline='', encoding='utf-8') as csv_file:
        csv_writer = csv.writer(csv_file)

        header = ['id_num', 'filepath']
        header.extend([f'duration_ns_{i}' for i in range(num_passes)])
        header.extend(['duration_ns_mean', 'duration_ns_median', 'duration_ns_fastest'])
        csv_writer.writerow(header)

        # Write data rows and calculate summary stats simultaneously
        for result_dict in results_list:
            # Extract required fields
            id_num = result_dict.get('id_num', '')
            filepath = result_dict.get('filepath', '')
            durations_ns = result_dict.get('duration_nanoseconds', [])

            mean_duration_ns_csv = ''
            median_duration_ns_csv = ''
            fastest_duration_ns_csv = ''
            mean_duration_ns_calc = 0.0
            fastest_duration_ns_calc = float('inf')

            if durations_ns:
                # Use integer durations directly where possible
                numeric_durations = [int(d) for d in durations_ns]
                total_cumulative_ns += sum(numeric_durations)

                # Calculate stats
                mean_duration_ns_calc = statistics.mean(numeric_durations)
                median_duration_ns_calc = statistics.median(numeric_durations)
                fastest_duration_ns_calc = min(numeric_durations)

                # Format for CSV
                mean_duration_ns_csv = f'{mean_duration_ns_calc:.0f}'
                median_duration_ns_csv = f'{median_duration_ns_calc:.0f}'
                fastest_duration_ns_csv = f'{fastest_duration_ns_calc:.0f}'

                # Add to summary totals
                total_mean_ns += mean_duration_ns_calc
                total_fastest_ns += fastest_duration_ns_calc

            # Construct the row
            row_data = [id_num, filepath]
            row_data.extend(durations_ns)
            row_data.append(mean_duration_ns_csv)
            row_data.append(median_duration_ns_csv)
            row_data.append(fastest_duration_ns_csv)

            csv_writer.writerow(row_data)

    # Print Summary Stats
    cumulative_time_s = total_cumulative_ns / 1_000_000_000.0
    avg_total_time_s = total_mean_ns / 1_000_000_000.0
    best_total_time_s = total_fastest_ns / 1_000_000_000.0
    print('\n--- Summary Statistics ---')
    print(f'Cumulative Render Time (All Passes): {cumulative_time_s:.1f} s')
    print(f'Estimated Single-Pass Time (Sum of Averages): {avg_total_time_s:.1f} s')
    print(f'Cumulative Best Time (Sum of Fastest Passes): {best_total_time_s:.1f} s')

    # Optional: Print completion message to stderr
    print(f"\nSuccessfully exported data (including stats) for {len(results_list)} entries to '{output_csv_filename}'.")


# Standard Python entry point guard
if __name__ == '__main__':
    main()
