# Syntalos Data Types and Time Sync Library

sy_datactl_pub_hdr = [
    'binarystream.h',
    'datatypes.h',
    'frametype.h',
    'edlstorage.h',
    'eigenaux.h',
    'logging.h',
    'streammeta.h',
    'syclock.h',
    'timesync.h',
    'tsyncfile.h',
    'uuid.h',
]

sy_datactl_priv_hdr = [
    'loginternal.h',
]

sy_datactl_src = [
    'datatypes.cpp',
    'edlstorage.cpp',
    'logging.cpp',
    'streammeta.cpp',
    'syclock.cpp',
    'timesync.cpp',
    'tsyncfile.cpp',
    'uuid.cpp',
]

sy_datactl_moc = qt.compile_moc(
    headers: sy_datactl_pub_hdr,
    extra_args: ['--no-notes']
)

syntalos_datactl_lib = shared_library('syntalos-datactl',
    [sy_datactl_pub_hdr,
     sy_datactl_priv_hdr,
     sy_datactl_src,
     sy_datactl_moc],
    soversion: sy_mlink_abi_level,
    version: sy_version,
    dependencies: [
        qt_core_dep,
        eigen_dep,
        xxhash_dep,
        opencv_dep,
        syntalos_utils_dep,
    ],
    include_directories: [
        root_include_dir,
        include_directories('../'),
    ],
    install: true,
    install_rpath: sy_libdir,
)

syntalos_datactl_dep = declare_dependency(
    link_with: syntalos_datactl_lib,
    dependencies: [
        qt_core_dep,
        eigen_dep,
    ],
    include_directories: [
        include_directories('./include'),
    ]
)

install_headers(sy_datactl_pub_hdr, subdir: 'syntalos' / 'datactl')
install_headers('include/syntalos-datactl', subdir: 'syntalos')

pkgc.generate(
    syntalos_datactl_lib,
    version: sy_version,
    name: 'syntalos-datactl',
    description: 'Library containing Syntalos data management, streaming and time synchronization primitives.',
    filebase: 'syntalos-datactl',
    subdirs: 'syntalos',
    requires: [
        'Qt6Core',
        'eigen3',
    ],
)
