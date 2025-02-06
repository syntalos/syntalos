# Build definition for Syntalos MetaView

syntalos_metaview_hdr = [
    'readtsync.h'
]

syntalos_metaview_src = [
    'main.cpp',
    'readtsync.cpp'
]

syntalos_metaview_exe = executable('syntalos-metaview',
    [syntalos_metaview_hdr,
     syntalos_metaview_src],
    dependencies: [syntalos_fabric_dep],
    install: true,
    install_rpath: sy_libdir
)
