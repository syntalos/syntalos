
encodehelper_hdr = [
    'encodewindow.h',
    '../videowriter.h',
    'queuemodel.h',
    'taskmanager.h',
    'encodetask.h',
]

encodehelper_src = [
    'encodewindow.cpp',
    '../videowriter.cpp',
    'queuemodel.cpp',
    'taskmanager.cpp',
    'encodetask.cpp',
]

encodehelper_ui = [
    'encodewindow.ui'
]

encodehelper_moc = qt.preprocess(
    moc_headers: encodehelper_hdr,
    moc_sources: encodehelper_src,
    ui_files: encodehelper_ui,
    qresources: ['encodehelper.qrc'],
    moc_extra_arguments: ['--no-notes']
)

executable('encodehelper',
    ['main.cpp',
     config_h,
     encodehelper_hdr,
     encodehelper_src,
     encodehelper_moc],
    include_directories: [root_include_dir],
    dependencies: [syntalos_shared_dep,
                   qt_core_dep,
                   qt_gui_dep,
                   qt_dbus_dep,
                   qt_svg_dep,

                   opencv_dep,
                   avcodec_dep,
                   avformat_dep,
                   avutil_dep,
                   swscale_dep],
    install: true,
    install_dir: mod_install_dir,
    install_rpath: sy_libdir
)
