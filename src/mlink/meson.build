# Syntalos IPC Module Link Interface

sy_mlink_pub_hdr = [
    'syntaloslink.h',
    'syntaloslinkmodule.h',
]

sy_mlink_priv_hdr = [
    'ipc-types-private.h',
]

sy_mlink_src = [
    'syntaloslink.cpp',
    'syntaloslinkmodule.cpp',
]

sy_mlink_moc = qt.preprocess(
    moc_headers: [sy_mlink_pub_hdr],
    dependencies: [qt_core_dep],
)

syntalos_mlink_lib = library('syntalos-mlink',
    [sy_mlink_src,
     sy_mlink_pub_hdr,
     sy_mlink_priv_hdr,
     sy_mlink_moc],
    soversion: sy_mlink_abi_level,
    version: sy_version,
    dependencies: [
        qt_core_dep,
        iceoryx_posh_dep,
        iceoryx_hoofs_dep,
        syntalos_datactl_dep,
        syntalos_utils_dep,
        vips_dep,
    ],
    include_directories: [],
    install: true,
    install_rpath: sy_libdir,
)

syntalos_mlink_dep = declare_dependency(
    link_with: syntalos_mlink_lib,
    sources: [
        sy_mlink_pub_hdr,
    ],
    dependencies: [
        qt_core_dep,
        syntalos_datactl_dep,
    ],
    include_directories: [
        include_directories('.'),
        include_directories('./include')
    ],
)

install_headers(sy_mlink_pub_hdr, subdir: 'syntalos' / 'mlink')
install_headers('include/syntalos-mlink', subdir: 'syntalos')

pkgc.generate(
    syntalos_mlink_lib,
    version: sy_version,
    name: 'syntalos-mlink',
    description: 'Library to build external Syntalos modules in separate processes.',
    filebase: 'syntalos-mlink',
    subdirs: 'syntalos',
    requires: [
        'Qt5Core',
        'syntalos-datactl'
    ]
)

# Meson adds (in our case) bogus Requires.private lines which break
# builds using libappstream. Fix this issue by post-processing the file.
mlink_pc_fixup = run_command(
    sed_prog,
    '-i',
    '/^Requires.private\|^Libs.private/ d',
    meson.global_build_root() / 'meson-private' / 'syntalos-mlink.pc',
    check: false
)
if mlink_pc_fixup.returncode() != 0
    error('Unable to fix up the pc file:\n' + pc_fixup.stderr())
endif
