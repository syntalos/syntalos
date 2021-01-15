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

sy_oop_hdr = [
    'oopmodule.h',
    'oopworkerconnector.h'
]
sy_oop_src = [
    'oopmodule.cpp',
    'oopworkerconnector.cpp'
]

sy_oop_shared_moc = qt.preprocess(
    moc_headers: sy_oop_shared_hdr,
    moc_sources: sy_oop_shared_src
)
sy_oop_moc = qt.preprocess(
    moc_headers: [sy_oop_hdr, rep_worker_rep_hdr],
    moc_sources: sy_oop_src,
    dependencies: [qt_remoteobj_dep],
    moc_extra_arguments: ['--no-notes']
)

sy_oopshared_lib = static_library('sy_oopshared',
    [sy_oop_shared_src, sy_oop_shared_moc],
    dependencies: [sy_base_deps,
                   qt_remoteobj_dep,
                   opencv_dep],
    include_directories: [include_directories('..')],
    cpp_args: ['-DNO_TID_PORTCONSTRUCTORS']
)

sy_oop_lib = static_library('sy_oop',
    [sy_oop_src, rep_worker_rep_hdr, sy_oop_moc],
    link_with: [sy_oopshared_lib],
    dependencies: [sy_base_deps,
                   qt_remoteobj_dep,
                   opencv_dep],
    include_directories: [include_directories('..')]
)
