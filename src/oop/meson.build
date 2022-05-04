# Out-of-process module glue code

syntalos_oop_inc_dir = include_directories('.')

repc_exe = find_program('repc')
rep_worker_rep_hdr = custom_target('gen-worker-qro-replica',
    input : ['interface.rep'],
    output : ['rep_interface_replica.h'],
    command : [repc_exe, '@INPUT0@',
                '-o', 'replica',
                '@OUTPUT0@']
)

sy_oop_shared_hdr = [
    'sharedmemory.h',
    'ipcmarshal.h',
]
sy_oop_shared_src = [
    'sharedmemory.cpp',
    'ipcmarshal.cpp'
]

sy_oop_moc = qt.preprocess(
    moc_headers: [rep_worker_rep_hdr],
    dependencies: [qt_remoteobj_dep],
    # FIXME: The forced include directive works around a weird issue in KDE's Flatpak runtime,
    # where some Qt modules are not found properly, resulting in parsing errors.
    moc_extra_arguments: ['-I/usr/include']
)

sy_oopshared_lib = static_library('sy_oop_shared',
    [sy_oop_shared_src, sy_oop_shared_hdr],
    dependencies: [sy_base_deps,
                   qt_remoteobj_dep,
                   opencv_dep],
    include_directories: [root_include_dir,
                          include_directories('..')],
    cpp_args: ['-DNO_TID_PORTCONSTRUCTORS']
)
