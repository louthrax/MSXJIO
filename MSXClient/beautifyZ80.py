#!/usr/bin/python3

import sys
import argparse
import glob
import os


def spaces_to_tabs(lines, tabsize=8):
    result = []
    for line in lines:
        new_line = ''
        col = 0
        i = 0
        while i < len(line):
            char = line[i]
            if char == ' ':
                # Compter le nombre d'espaces consécutifs
                start = i
                while i < len(line) and line[i] == ' ':
                    i += 1
                space_count = i - start

                # Ajouter des tabs si possible, puis compléter par des espaces
                while space_count > 0:
                    spaces_to_tab = tabsize - (col % tabsize)
                    if space_count >= spaces_to_tab:
                        new_line += '\t'
                        col += spaces_to_tab
                        space_count -= spaces_to_tab
                    else:
                        new_line += ' ' * space_count
                        col += space_count
                        space_count = 0
            else:
                new_line += char
                col += 1
                i += 1
        result.append(new_line)
    return result


def align_asm_lines(lines, tab1=8, tab2=16, tab3=32):
    aligned_lines = []
    for line in lines:
        stripped = line.rstrip('\n')

        if stripped.startswith(';'):
            aligned_lines.append(stripped)
            continue

        if stripped.lstrip().startswith(';'):
            comment = stripped.lstrip()
            aligned_lines.append(''.ljust(tab1) + comment)
            continue

        if not stripped.strip():
            aligned_lines.append('')
            continue

        label, instr, operand, comment = '', '', '', ''
        parts = stripped.split(';', 1)
        code_part = parts[0]
        comment = ';' + parts[1] if len(parts) > 1 else ''

        tokens = code_part.strip().split()

        if not tokens:
            aligned_lines.append(comment.rjust(tab3) if comment else '')
            continue

        if stripped.startswith(' ') or stripped.startswith('\t'):
            if len(tokens) >= 1:
                instr = tokens[0]
            if len(tokens) >= 2:
                operand = ' '.join(tokens[1:])
        else:
            label = tokens[0]
            if len(tokens) >= 2:
                instr = tokens[1]
            if len(tokens) >= 3:
                operand = ' '.join(tokens[2:])

        line_out = label.ljust(tab1)
        if instr:
            line_out += instr.ljust(tab2 - len(line_out))
        line_out += operand.ljust(tab3 - len(line_out))
        if comment:
            line_out += comment

        aligned_lines.append(line_out.rstrip())

    return aligned_lines

def main():
    parser = argparse.ArgumentParser(description="Align columns in Z80 ASM files.")
    parser.add_argument("files", nargs='+', help="Input .asm file(s) (wildcards supported)")
    parser.add_argument("--tab1", type=int, default=24, help="Tab stop after label (default: 24)")
    parser.add_argument("--tab2", type=int, default=40, help="Tab stop after instruction (default: 40)")
    parser.add_argument("--tab3", type=int, default=80, help="Tab stop after operand (default: 80)")

    args = parser.parse_args()

    matched_files = []
    for pattern in args.files:
        matched = glob.glob(pattern)
        if not matched:
            print(f"No match found for: {pattern}")
        matched_files.extend(matched)

    if not matched_files:
        print("No files to process.")
        sys.exit(1)

    for file in matched_files:
        with open(file, 'r') as f:
            lines = f.readlines()

        aligned = align_asm_lines(lines, args.tab1, args.tab2, args.tab3)
        #aligned = spaces_to_tabs(aligned, 8)        

        with open(file, 'w') as f:
            f.write('\n'.join(aligned))

        print(f"✅ {file} aligned.")

if __name__ == "__main__":
    main()
