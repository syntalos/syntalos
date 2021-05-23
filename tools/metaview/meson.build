# Build definition for Syntalos MetaView

syntalos_metaview_hdr = [
    'readtsync.h'
]
syntalos_metaview_moc_hdr = []

syntalos_metaview_src = [
    'main.cpp',
    'readtsync.cpp'
]
syntalos_metaview_moc_src = []

syntalos_metaview_moc = qt.preprocess(
    moc_headers: syntalos_metaview_moc_hdr,
    moc_sources: syntalos_metaview_moc_src
)

syntalos_metaview_exe = executable('syntalos-metaview',
    [syntalos_metaview_hdr, syntalos_metaview_moc_hdr,
     syntalos_metaview_src, syntalos_metaview_moc_src,
     syntalos_metaview_moc],
    dependencies: [syntalos_shared_dep],
    install: true,
    install_rpath: sy_libdir
)
