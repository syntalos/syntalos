# Syntalos Data Types and Time Sync Library

# This library contains common data types and data manipulation
# code used by Syntalos and its modules. It is public API.
#
# It also contains some private interfaces that the public
# (and private) Syntalos libraries may use internally.

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
    'edlutils.h',
    'loginternal.h',

    'priv/rtkit.h',
    'priv/cpuaffinity.h',
]

sy_datactl_src = [
    'datatypes.cpp',
    'edlstorage.cpp',
    'edlutils.cpp',
    'logging.cpp',
    'streammeta.cpp',
    'syclock.cpp',
    'timesync.cpp',
    'tsyncfile.cpp',
    'uuid.cpp',

    'priv/cpuaffinity.cpp',
    'priv/rtkit.cpp',
]

syntalos_datactl_lib = shared_library('syntalos-datactl',
    [sy_datactl_pub_hdr,
     sy_datactl_priv_hdr,
     sy_datactl_src],
    soversion: sy_mlink_abi_level,
    version: sy_version,
    dependencies: [
        gio_dep,
        eigen_dep,
        xxhash_dep,
        opencv_dep,
        toml_dep,
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
        eigen_dep,
    ],
    include_directories: [
        include_directories('./include'),
        include_directories('../'),
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
        'eigen3',
    ],
)
