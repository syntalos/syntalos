# Syntalos Python Integration

# try to find NumPy
python_inc_args = run_command('python3',
                               ['-c',
                                'import site\n' +
                                'import sys\n' +
                                'for p in site.getsitepackages() + sys.path:\n' +
                                '    if p: print("-I" + p + "/numpy/_core/include\\n" + "-I" + p + "/numpy/core/include")'],
                               check: true).stdout().strip().split()

have_numpy = false
foreach inc_arg : python_inc_args
    if cpp_compiler.has_argument(inc_arg) and \
        cpp_compiler.check_header('numpy/arrayobject.h',
                                  args: inc_arg,
                                  dependencies: python_dep)
        numpy_inc_arg = inc_arg
        have_numpy = true
        break
    endif
endforeach
if not have_numpy
    error('Unable to find Python\'s NumPy module. Please install it to continue.')
endif

pysy_mlink_src = [
    'pysy-mlink.h',
    'pysy-mlink.cpp',
    'sydatatopy.h',

    'cvnp/cvnp.cpp',
    'cvnp/cvnp.h',
    'cvnp/cvnp_synonyms.cpp',
    'cvnp/cvnp_synonyms.h',
    'cvnp/pydef_cvnp.cpp',
]

pysy_mlink_mod = python.extension_module(
    'syntalos_mlink',
    pysy_mlink_src,
    gnu_symbol_visibility: 'hidden',
    dependencies: [
        syntalos_mlink_dep,
        thread_dep,
        glib_dep,
        opencv_dep,
        pybind11_dep,
        python_embed_dep,
    ],
    include_directories: [include_directories('..')],
    cpp_args: [
        numpy_inc_arg,
        glib_api_limit_flags,
    ],
    install: true,
)

test_cvnp_exe = executable('test-cvnp',
    ['cvnp/cvnp.cpp',
     'cvnp/cvnp.h',
     'cvnp/cvnp_synonyms.cpp',
     'cvnp/cvnp_synonyms.h',
     'cvnp/pydef_cvnp.cpp',
     'cvnp/cvnp_test_helper.cpp',
     'cvnp/tests/test_cvnp_cpp.cpp'],
    dependencies: [
        thread_dep,
        opencv_dep,
        pybind11_dep,
        python_embed_dep,
    ],
    cpp_args: ['-Wno-unused-function'],
)
test('sy-test-cvnp-cpp',
     test_cvnp_exe,
     args: python.get_path('purelib')
)

test_sydatatopy_exe = executable('test-sydatatopy',
    ['tests/test-sydatatopy.cpp'],
    dependencies: [
        syntalos_mlink_dep,
        pybind11_dep,
        python_embed_dep,
    ],
    include_directories: [include_directories('..')],
)
test('sy-test-sydatatopy', test_sydatatopy_exe)

pb11_stubgen_exe = find_program('pybind11-stubgen', required: false)
if pb11_stubgen_exe.found()
    pysy_mlink_stub = custom_target('pysy-mlink-stubgen',
        build_by_default: true,
        command: [
            pb11_stubgen_exe,
            '--numpy-array-wrap-with-annotated',
            '-o', meson.current_build_dir(),
            'syntalos_mlink'
        ],
        depends: [pysy_mlink_mod],
        input: pysy_mlink_mod,
        output: 'syntalos_mlink.pyi',
        env: {'PYTHONPATH': meson.current_build_dir()},
        install: true,
        install_dir: python.get_install_dir(),
    )
endif

# Generate some documentation files for the embedded scripting interface, if we can
make_pydocs_exe = find_program('make-pydocs.py')
have_python_pdoc = run_command(python, make_pydocs_exe.full_path(), 'have-pdoc', check: false).returncode() == 0
if have_python_pdoc and pb11_stubgen_exe.found()
    pysy_mlink_doc_fname = source_root / 'docs' / 'pysy_mlink_api_embed.html'
    custom_target('pysy-mlink-mkdoc',
        build_by_default: true,
        input: [pysy_mlink_stub],
        command: [
            python, make_pydocs_exe.full_path(),
            'generate',
            '--file', '@INPUT0@',
            '@OUTPUT0@'
        ],
        output: 'pysy_mlink_api_embed.html',
    )
endif
