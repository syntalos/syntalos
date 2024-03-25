# PyWorker helper process

# try to find NumPy
python_inc_args = run_command('python3',
                               ['-c',
                                'import site\n' +
                                'import sys\n' +
                                'for i in site.getsitepackages() + sys.path:\n' +
                                '    if i: print("-I" + i + "/numpy/core/include")'],
                               check: true).stdout().strip().split()
python_inc_args += '-I' + run_command('python3',
                                      ['-m', 'site', '--user-site'],
                                      check: true).stdout().strip() + '/numpy/core/include'

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

rep_worker_src_hdr = custom_target('gen-worker-qro-source',
    input : ['../fabric/oop/interface.rep'],
    output : ['rep_interface_source.h'],
    command : [repc_exe, '@INPUT0@',
                '-o', 'source',
                '@OUTPUT0@']
)

sy_pyworker_src = [
    'main.cpp',
    'worker.h',
    'worker.cpp',
    'syio.h',
    'syio.cpp',
    'qstringtopy.h',
    'cvmatndsliceconvert.h',
    'cvmatndsliceconvert.cpp',
    'pyipcmarshal.h',
    'pyipcmarshal.cpp'
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

sy_pyworker_moc = qt.preprocess(
    moc_headers: [pyworker_moc_h, rep_worker_src_hdr],
    moc_sources: pyworker_moc_s,
    dependencies: [qt_remoteobj_dep],
    # FIXME: The forced include directive works around a weird issue in KDE's Flatpak runtime,
    # where some Qt modules are not found properly, resulting in parsing errors.
    moc_extra_arguments: ['-I/usr/include', '--no-notes']
)

pyworker_exe = executable('pyworker',
    [sy_pyworker_src, sy_pyworker_moc, rep_worker_src_hdr],
    gnu_symbol_visibility: 'hidden',
    link_with: [sy_oopshared_lib],
    dependencies: [syntalos_fabric_dep,
                   thread_dep,
                   qt_remoteobj_dep,
                   qt_dbus_dep,
                   opencv_dep,
                   vips_dep,
                   pybind11_dep,
                   python_embed_dep],
    include_directories: [include_directories('..'),
                          include_directories('../fabric/oop')],
    cpp_args: ['-DNO_TID_PORTCONSTRUCTORS', numpy_inc_arg],
    install: true,
    install_dir: sy_libdir
)

# Generate some documentation files for the embedded scripting interface,
# if we can
pdoc_exe = find_program('pdoc', required: false)
if pdoc_exe.found()
    # the pdoc Python module is probably there, so we can try to refresh the documentation
    syio_doc_fname = source_root / 'docs' / 'syio_api_embed.html'
    custom_target('pyworker-mkdoc',
        build_by_default: true,
        command : [pyworker_exe,
                    '--doc',
                    '@OUTPUT0@'],
        output: 'syio_api_embed.html',
        env: ['LSAN_OPTIONS=suppressions=' +
                join_paths(source_root, 'tests', 'lsan-suppr.txt')]
    )
endif
