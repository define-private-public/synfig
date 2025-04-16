#!/usr/bin/env python3

import sys
from pathlib import Path
import os # Imported for os.path.abspath for clarity, though Path.resolve() is used

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
    Main function to handle command-line arguments and print the results.
    """
    # Check if a command-line argument (directory path) was provided
    if len(sys.argv) < 2:
        print("Usage: python find_sif_files.py <directory_path>", file=sys.stderr)
        # Provide the script name dynamically
        # print(f"Usage: {sys.argv[0]} <directory_path>", file=sys.stderr)
        sys.exit(1) # Exit with an error code

    # Get the directory path from the command-line arguments
    target_directory = sys.argv[1]

    # Find the .sif files
    found_files = find_sif_files_recursively(target_directory)

    # Print the list of found absolute paths, one per line
    if found_files:
        print("\nFound .sif files:")
        for file_path in found_files:
            print(file_path)
    else:
        print("\nNo .sif files found.")

# Standard Python entry point guard
if __name__ == "__main__":
    main()

