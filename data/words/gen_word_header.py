#!/usr/bin/env python3
"""Generate a single C header with constexpr byte-arrays from multiple text files.

Usage: gen_word_header.py <output.h> <var_name>:<input.txt> [...]
"""
import sys


def emit_array(f, var_name, data):
    f.write('constexpr unsigned char {}[] = {{\n'.format(var_name))
    for i, b in enumerate(data):
        if i % 16 == 0:
            f.write('    ')
        f.write('0x{:02x},'.format(b))
        if i % 16 == 15:
            f.write('\n')
        else:
            f.write(' ')
    if data and len(data) % 16 != 0:
        f.write('\n')
    f.write('};\n')


outfile = sys.argv[1]
pairs = sys.argv[2:]  # each "var_name:path"

with open(outfile, 'w') as out:
    out.write('// Auto-generated — do not edit\n')
    out.write('// clang-format off\n')
    for pair in pairs:
        var_name, infile = pair.split(':', 1)
        with open(infile, 'rb') as f:
            data = f.read()
        emit_array(out, var_name, data)
    out.write('// clang-format on\n')
