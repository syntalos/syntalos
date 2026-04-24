# Python worker process for the PyScript module

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

pyworker_exe = executable('pyworker',
    [sy_pyworker_src,
     sy_pyworker_moc,
     config_h],
    gnu_symbol_visibility: 'hidden',
    dependencies: [
        syntalos_mlink_dep,
        thread_dep,
        qt_dbus_dep,
        qt_gui_dep,
        pybind11_dep,
        python_embed_dep
    ],
    include_directories: [include_directories('..'), root_include_dir],
    install: true,
    install_dir: pysmod_install_dir,
    install_rpath: sy_libdir,
)
