# Configuration file for the Sphinx documentation builder.

import textwrap
from jupyter_sphinx_theme import *
init_theme()

# -- Project information -----------------------------------------------------

project = 'Syntalos'
copyright = '2018-2020, Matthias Klumpp'
author = 'Matthias Klumpp'

# The full version, including alpha/beta/rc tags
release = '0.1'

# -- General configuration ---------------------------------------------------

extensions = [
    'breathe',
    'exhale'
]

# Setup the breathe extension
breathe_projects = {
    'Syntalos': './doxyoutput/xml'
}
breathe_default_project = 'Syntalos'

# Setup the exhale extension
exhale_args = {
    # These arguments are required
    'containmentFolder':     './api',
    'rootFileName':          'sysrc_root.rst',
    'rootFileTitle':         'Syntalos API',
    'doxygenStripFromPath':  '../src',
    'createTreeView':        True,
    'exhaleExecutesDoxygen': True,
    'exhaleDoxygenStdin':    textwrap.dedent('''
                                INPUT                = ../src
                                BUILTIN_STL_SUPPORT  = YES
                                EXTRACT_PRIVATE      = NO
                                #EXTRACT_PRIV_VIRTUAL = YES
                                EXCLUDE_PATTERNS     = *.txt \
                                                       *.md \
                                                       *elidedlabel* \
                                                       *rangeslider*
                                INCLUDE_FILE_PATTERNS = *.h *.hpp
                                EXTENSION_MAPPING    = h=C++
                                ENABLE_PREPROCESSING = YES
                                RECURSIVE            = YES
                                EXCLUDE_SYMBOLS      = *::Private \
                                                       Q_DECLARE_*
                                EXTRACT_LOCAL_CLASSES  = NO
                                FORCE_LOCAL_INCLUDES   = YES
                                CLANG_ASSISTED_PARSING = YES
                                ''')
}

# Tell sphinx what the primary language being documented is.
primary_domain = 'cpp'

# Tell sphinx what the pygments highlight language should be.
highlight_language = 'cpp'

templates_path = ['_templates']

exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']


# -- Options for HTML output -------------------------------------------------

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = ['_static']
