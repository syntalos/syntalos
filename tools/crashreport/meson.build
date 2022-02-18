# Build definition for the Syntalos Crash Report Helper

sy_crashreport_hdr = [
    'journal-collect.h'
]
sy_crashreport_moc_hdr = [
    'crashreportdialog.h',
]

sy_crashreport_src = [
    'main.cpp',
    'crashreportdialog.cpp',
    'journal-collect.cpp',
]
sy_crashreport_moc_src = [
]

sy_crashreport_ui = [
    'crashreportdialog.ui'
]

sy_crashreport_moc = qt.preprocess(
    moc_headers: sy_crashreport_moc_hdr,
    moc_sources: sy_crashreport_moc_src,
    ui_files: sy_crashreport_ui,
    dependencies: [qt_core_dep,
                   qt_gui_dep]
)

executable('syntalos-crashreport',
    [sy_crashreport_hdr, sy_crashreport_moc_hdr,
     sy_crashreport_src, sy_crashreport_moc_src,
     sy_crashreport_moc],
    dependencies: [qt_core_dep,
                   qt_gui_dep,
                   libsystemd_dep],
    install_dir: get_option('libexecdir'),
    install: true
)
