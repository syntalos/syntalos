# Build definition for the Syntalos Crash Report Helper

sy_crashreport_hdr = [
    'debugcollect.h',
    'utils.h',
]
sy_crashreport_moc_hdr = [
    'crashreportdialog.h',
]

sy_crashreport_src = [
    'main.cpp',
    'crashreportdialog.cpp',
    'debugcollect.cpp',
    'utils.cpp',
    '../../src/appstyle.cpp',
    '../../src/utils/style.cpp',
]
sy_crashreport_moc_src = [
]

sy_crashreport_ui = [
    'crashreportdialog.ui'
]

sy_crashreport_res = ['resources.qrc']

sy_crashreport_moc = qt.compile_moc(
    headers: sy_crashreport_moc_hdr,
    sources: sy_crashreport_moc_src,
    dependencies: [qt_core_dep,
                   qt_gui_dep,
                   qt_concurrent_dep],
)
sy_crashreport_moc += qt.compile_ui(sources: sy_crashreport_ui)
sy_crashreport_moc += qt.compile_resources(sources: sy_crashreport_res)

executable('syntalos-crashreport',
    [sy_crashreport_hdr, sy_crashreport_moc_hdr,
     sy_crashreport_src, sy_crashreport_moc_src,
     sy_crashreport_moc],
    dependencies: [qt_core_dep,
                   qt_gui_dep,
                   qt_concurrent_dep,
                   libsystemd_dep,
                   kfarchive_dep,
                   kfconfigwidgets_dep],
    include_directories: [root_include_dir,
                          include_directories('../../src')],
    install_dir: get_option('libexecdir'),
    install: true
)
