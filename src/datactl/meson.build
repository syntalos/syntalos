# Syntalos Data Types and Time Sync Library

sy_datactl_pub_hdr = [
    'datatypes.h',
    'frametype.h',
    'edlstorage.h',
    'eigenaux.h',
    'syclock.h',
    'timesync.h',
    'tsyncfile.h',
    'vips8-q.h',
    'vipsutils.h',
]

sy_datactl_priv_hdr = [

]

sy_datactl_src = [
    'datatypes.cpp',
    'edlstorage.cpp',
    'syclock.cpp',
    'timesync.cpp',
    'tsyncfile.cpp',
    'vipsutils.cpp',
]

sy_datactl_inc_dir = include_directories('..')

sy_datactl_moc = qt.preprocess(
    moc_headers: sy_datactl_pub_hdr,
    moc_extra_arguments: ['--no-notes']
)

syntalos_datactl_lib = shared_library('syntalos-datactl',
    [sy_datactl_pub_hdr,
     sy_datactl_priv_hdr,
     sy_datactl_src,
     sy_datactl_moc],
    soversion: sy_mlink_abi_level,
    version: sy_version,
    dependencies: [sy_base_deps,
                   eigen_dep,
                   xxhash_dep,
                   vips_dep,
                   opencv_dep,
                   syntalos_utils_dep,
    ],
    include_directories: [root_include_dir,
                          sy_datactl_inc_dir],
    install: true,
    install_rpath: sy_libdir,
)

syntalos_datactl_dep = declare_dependency(
    link_with: syntalos_datactl_lib,
    dependencies: [
        sy_base_deps,
        eigen_dep,
        syntalos_utils_dep
    ],
    include_directories: [sy_datactl_inc_dir]
)

install_headers(sy_datactl_pub_hdr, subdir: 'syntalos' / 'datactl')

pkgc.generate(
    syntalos_datactl_lib,
    version: sy_version,
    name: 'syntalos-datactl',
    description: 'Library containing Syntalos data management, streaming and time synchronization primitives.',
    filebase: 'syntalos-datactl',
    subdirs: 'syntalos',
    requires: [
        'Qt5Core',
        'eigen3',
    ]
)
