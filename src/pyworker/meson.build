# PyWorker helper process


# try to find NumPy
python_inc_args = python_dep.get_variable(pkgconfig: 'Cflags').strip().split()
python_inc_args += run_command('python3', ['-c', 'import site\nfor i in site.getsitepackages(): print("-I" + i + "/numpy/core/include")']).stdout().strip().split()
python_inc_args += '-I' + run_command('python3', ['-m', 'site', '--user-site']).stdout().strip() + '/numpy/core/include'

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
    error('Unable to found Python\'s NumPy module. Please install it to continue.')
endif

rep_worker_src_hdr = custom_target('gen-worker-qro-source',
    input : ['../oop/interface.rep'],
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
    '../rtkit.h',
    '../rtkit.cpp',
    '../cpuaffinity.h',
    '../cpuaffinity.cpp',
    '../syclock.h',
    '../syclock.cpp',
    '../streams/datatypes.h',
    '../streams/datatypes.cpp',
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
    moc_extra_arguments: ['--no-notes']
)

pyworker_exe = executable('pyworker',
    [sy_pyworker_src, sy_pyworker_moc, rep_worker_src_hdr],
    gnu_symbol_visibility: 'hidden',
    link_with: [sy_oopshared_lib],
    dependencies: [sy_base_deps,
                   thread_dep,
                   qt_remoteobj_dep,
                   qt_dbus_dep,
                   opencv_dep,
                   pybind11_dep,
                   python_embed_dep],
    include_directories: [include_directories('..'),
                          include_directories('../oop')],
    cpp_args: ['-DNO_TID_PORTCONSTRUCTORS', numpy_inc_arg],
    install: true,
    install_dir: sy_libdir
)

# Generate some documentation files for the embedded scripting interface,
# if we can

#find_program(PDOC_EXE pdoc)
#if (PDOC_EXE)
#    # the pdoc Python module is probably there, so we can try to refresh the documentation
#    set(syio_doc_fname ${CMAKE_SOURCE_DIR}/docs/modules/pyapi_syio.rst)
#    add_custom_command(
#      DEPENDS pyworker
#      COMMAND pyworker --doc ${syio_doc_fname}
#      COMMAND ${CMAKE_COMMAND} -E touch syio_doc_gen.stamp
#      OUTPUT syio_doc_gen.stamp
#      COMMENT "Extracting embedded Python documentation"
#      VERBATIM
#    )
#
#    add_custom_target(
#      sysio-pdoc ALL
#      DEPENDS syio_doc_gen.stamp
#    )
#endif()
