#!/usr/bin/python3

import re
import sys

def clean_iar_asm(input_path, output_path):
    with open(input_path, 'r') as f:
        lines = f.readlines()

    cleaned_lines = []
    found_label = False

    for line in lines:
        stripped = line.strip()

        # Start processing only after the first label (e.g., Something:)
        if not found_label and stripped.endswith(':'):
            found_label = True

        if found_label:
            # Remove the END line if present
            if stripped.upper() == 'END':
                continue

            # Replace all ?XYZ labels with _XYZ
            line = re.sub(r'\?(\w+)', r'_\1', line)
            cleaned_lines.append(line)

    with open(output_path, 'w') as f:
        f.writelines(cleaned_lines)

def main():
    if len(sys.argv) != 3:
        print("Usage: python clean_asm.py <input.asm> <output.asm>")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]
    clean_iar_asm(input_file, output_file)
    print(f"Cleaned assembly written to: {output_file}")

if __name__ == "__main__":
    main()
