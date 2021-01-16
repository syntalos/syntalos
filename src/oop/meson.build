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

sy_oop_shared_moc = qt.preprocess(
    moc_headers: sy_oop_shared_hdr
)

sy_oop_moc = qt.preprocess(
    moc_headers: [rep_worker_rep_hdr],
    dependencies: [qt_remoteobj_dep]
)

sy_oopshared_lib = static_library('sy_oop_shared',
    [sy_oop_shared_src, sy_oop_shared_moc],
    dependencies: [sy_base_deps,
                   qt_remoteobj_dep,
                   opencv_dep],
    include_directories: [root_include_dir,
                          include_directories('..')],
    cpp_args: ['-DNO_TID_PORTCONSTRUCTORS']
)
