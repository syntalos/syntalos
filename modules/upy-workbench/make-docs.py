#!/usr/bin/env python3
#
# Copyright (C) 2023-2024 Matthias Klumpp <matthias@tenstral.net>
#
# SPDX-License-Identifier: LGPL-3.0+

import os
import sys
import tempfile
import pdoc


jinjaTmpl = """
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


def generate_docs(root_dir, output_filename):
    # Create a temporary directory to store the template
    with tempfile.TemporaryDirectory(delete=True) as tmp_dir:
        tpl_dir = os.path.join(tmp_dir, 'tmpl')
        os.makedirs(tpl_dir, exist_ok=True)
        with open(os.path.join(tpl_dir, 'frame.html.jinja2'), 'w') as f:
            f.write(jinjaTmpl)

        with open(os.path.join(root_dir, 'upy-comms.py'), 'r') as sf:
            with open(os.path.join(tmp_dir, 'upy-comms.py'), 'w') as df:
                for line in sf.readlines():
                    if not line.startswith('import uasyncio'):
                        df.write(line)

        pdoc.render.configure(template_directory=tpl_dir)

        # Generate documentation
        doc = pdoc.doc.Module.from_name(
            pdoc.extract.parse_spec(os.path.join(tmp_dir, 'upy-comms.py'))
        )
        html_data = pdoc.render.html_module(module=doc, all_modules={'upy_comms': doc})

        # Write the generated HTML documentation to the output file
        with open(output_filename, 'w') as f:
            for line in html_data.split('\n'):
                f.write(line.strip() + '\n')
            f.write('\n')


if __name__ == "__main__":
    thisfile = os.path.realpath(__file__)
    if not os.path.isabs(thisfile):
        thisfile = os.path.normpath(os.path.join(os.getcwd(), thisfile))
    thisdir = os.path.normpath(os.path.join(os.path.dirname(thisfile)))

    if len(sys.argv) < 2:
        print('No output filename was specified!')
        sys.exit(4)

    generate_docs(thisdir, sys.argv[1])
