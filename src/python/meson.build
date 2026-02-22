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
    'qstringtopy.h',

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
        syntalos_fabric_dep,
        syntalos_mlink_dep,
        thread_dep,
        opencv_dep,
        pybind11_dep,
        python_embed_dep,
    ],
    include_directories: [include_directories('..')],
    cpp_args: [numpy_inc_arg],
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

sy_pyworker_src = [
    'main.cpp',
    'pyworker.h',
    'pyworker.cpp',
]

pyworker_moc_h = []
pyworker_moc_s = []
foreach s : sy_pyworker_src
    if s.endswith('.h')
        pyworker_moc_h += s
    elif s.endswith('.cpp')
        pyworker_moc_s += s
    endif
endforeach

sy_pyworker_moc = qt.compile_moc(
    headers: [pyworker_moc_h],
    sources: pyworker_moc_s,
    # FIXME: The forced include directive works around a weird issue in KDE's Flatpak runtime, where
    # some Qt modules are not found properly without the explicit include, resulting in parsing errors.
    extra_args: ['-I/usr/include', '--no-notes']
)

pywconf = configuration_data()
pywconf.set_quoted('SY_PYTHON_MOD_DIR', python.get_install_dir(pure: false))
pyw_config_h = configure_file(output: 'pyw-config.h', configuration: pywconf)

pyworker_exe = executable('pyworker',
    [pyw_config_h,
     sy_pyworker_src,
     sy_pyworker_moc],
    gnu_symbol_visibility: 'hidden',
    dependencies: [
        syntalos_mlink_dep,
        thread_dep,
        qt_dbus_dep,
        qt_gui_dep,
        pybind11_dep,
        python_embed_dep
    ],
    include_directories: [include_directories('..')],
    cpp_args: [numpy_inc_arg],
    install: true,
    install_dir: sy_libdir,
)

# Generate some documentation files for the embedded scripting interface,
# if we can
pdoc_exe = find_program('pdoc', required: false)
if pdoc_exe.found()
    # the pdoc Python module is probably there, so we can try to refresh the documentation

    pmd_env = environment()
    pmd_env.set('LSAN_OPTIONS', 'suppressions=' + join_paths(source_root, 'tests', 'lsan-suppr.txt'))
    pmd_env.set('SYNTALOS_MODULE_ID', 'build_docgen')

    # If we have Xvfb, use it - pyworker is a GUI application,
    # and this ensures it will complete even in a headless environment.
    xvfb_exe = find_program('Xvfb', required: false)
    xdpyinfo_exe = find_program('xdpyinfo', required: false)
    if xvfb_exe.found() and xdpyinfo_exe.found()
        xvfb_run_sh = files(source_root / 'tools' / 'xvfb-run.sh')
        pmd_command = [xvfb_run_sh,
                       '-a', '-s', '-screen 0 1024x768x24 +extension GLX',
                       pyworker_exe.full_path(), '--doc', '@OUTPUT0@']
    else
        pmd_command = [pyworker_exe, '--doc', '@OUTPUT0@']
    endif

    # refresh documentation
    pysy_mlink_doc_fname = source_root / 'docs' / 'pysy_mlink_api_embed.html'
    custom_target('pysy-mlink-mkdoc',
        build_by_default: true,
        command: pmd_command,
        depends: [pyworker_exe, pysy_mlink_mod],
        output: 'pysy_mlink_api_embed.html',
        env: pmd_env,
    )
endif
