#!/usr/bin/env python3
#
# Copyright (C) 2023-2026 Matthias Klumpp <matthias@tenstral.net>
#
# SPDX-License-Identifier: LGPL-3.0+

import argparse
import importlib
import os
import sys
import tempfile

JINJA_TEMPLATE = """
<div>
    {% block content %}{% endblock %}

    {% filter minify_css %}
        {% block style %}
            <style>{% include "syntax-highlighting.css" %}</style>
            <style>{% include "theme.css" %}</style>
            <style>{% include "content.css" %}</style>
        {% endblock %}
    {% endfilter %}
</div>
"""


def check_pdoc_available():
    try:
        import pdoc  # noqa: F401
    except ImportError:
        print('pdoc not found.')
        return 1

    print('pdoc is available.')
    return 0


def _write_html_output(fname, html_data):
    with open(fname, 'w') as f:
        for line in html_data.split('\n'):
            f.write(line.strip() + '\n')
        f.write('\n')


def generate_docs_file(source_file, out_fname, show_source=True):
    import pdoc

    source_file = os.path.abspath(source_file)

    # temporary directory for the template
    with tempfile.TemporaryDirectory() as tmp_dir:
        tpl_dir = os.path.join(tmp_dir, 'tmpl')
        os.makedirs(tpl_dir, exist_ok=True)
        with open(os.path.join(tpl_dir, 'frame.html.jinja2'), 'w') as f:
            f.write(JINJA_TEMPLATE)

        staged_basename = os.path.basename(source_file)
        if staged_basename.endswith('.pyi'):
            # pdoc expects an importable Python module path, so stage stubs as .py files.
            staged_basename = staged_basename[:-1]
            show_source = False
        staged_source = os.path.join(tmp_dir, staged_basename)
        with open(source_file, 'r') as sf:
            with open(staged_source, 'w') as df:
                for line in sf.readlines():
                    # pdoc runs on CPython, so rewrite MicroPython uasyncio imports/usages
                    if line == 'import uasyncio\n':
                        df.write('import asyncio as uasyncio\n')
                    else:
                        df.write(line)

        pdoc.render.configure(template_directory=tpl_dir, search=False, show_source=show_source)

        # generate documentation
        doc = pdoc.doc.Module.from_name(pdoc.extract.parse_spec(staged_source))
        html_data = pdoc.render.html_module(module=doc, all_modules={doc.fullname: doc})

        # write the generated HTML documentation to the output file
        _write_html_output(out_fname, html_data)


def generate_docs_module(module_name, out_fname, show_source=True):
    import pdoc

    imported_module = importlib.import_module(module_name)
    doc = pdoc.doc.Module(imported_module)

    with tempfile.TemporaryDirectory() as tmp_dir:
        with open(os.path.join(tmp_dir, 'frame.html.jinja2'), 'w') as f:
            f.write(JINJA_TEMPLATE)
        pdoc.render.configure(template_directory=tmp_dir, search=False, show_source=show_source)
        html_data = pdoc.render.html_module(module=doc, all_modules={doc.fullname: doc})
        _write_html_output(out_fname, html_data)


def main(argv, script_dir):
    parser = argparse.ArgumentParser(
        description='Generate pdoc documentation for modules or source files'
    )
    subparsers = parser.add_subparsers(dest='command', required=True)

    gen_parser = subparsers.add_parser('generate', help='Generate API documentation')
    gen_target = gen_parser.add_mutually_exclusive_group()
    gen_target.add_argument(
        '--module', dest='module_name', help='Importable module name to document'
    )
    gen_target.add_argument('--file', dest='file_path', help='Python source file to document')
    gen_parser.add_argument('output_filename', help='Output HTML filename')

    subparsers.add_parser('have-pdoc', help='Check whether pdoc is installed and importable')

    args = parser.parse_args(argv)

    if args.command == 'have-pdoc':
        return check_pdoc_available()

    try:
        if args.module_name:
            generate_docs_module(args.module_name, args.output_filename)
        elif args.file_path:
            generate_docs_file(args.file_path, args.output_filename)
        else:
            print('No module or file given!')
            return 1
    except Exception as e:
        print(e)
        return 1

    return 0


if __name__ == "__main__":
    thisfile = os.path.realpath(__file__)
    if not os.path.isabs(thisfile):
        thisfile = os.path.normpath(os.path.join(os.getcwd(), thisfile))
    thisdir = os.path.normpath(os.path.join(os.path.dirname(thisfile)))

    sys.exit(main(sys.argv[1:], thisdir))
