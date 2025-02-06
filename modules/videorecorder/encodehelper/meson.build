
encodehelper_hdr = [
    'encodewindow.h',
    '../videowriter.h',
    'queuemodel.h',
    'taskmanager.h',
    'encodetask.h',
    'videoreader.h',
]

encodehelper_src = [
    'encodewindow.cpp',
    '../videowriter.cpp',
    'queuemodel.cpp',
    'taskmanager.cpp',
    'encodetask.cpp',
    'videoreader.cpp',
]

encodehelper_ui = [
    'encodewindow.ui'
]

encodehelper_moc = qt.compile_moc(
    headers: encodehelper_hdr,
    sources: encodehelper_src,
    extra_args: ['--no-notes'],
)
encodehelper_moc += qt.compile_ui(sources: encodehelper_ui)
encodehelper_moc += qt.compile_resources(sources: ['encodehelper.qrc'])

executable('encodehelper',
    ['main.cpp',
     config_h,
     encodehelper_hdr,
     encodehelper_src,
     encodehelper_moc],
    include_directories: [root_include_dir],
    dependencies: [syntalos_fabric_dep,
                   qt_core_dep,
                   qt_gui_dep,
                   qt_dbus_dep,
                   qt_svg_dep,

                   libsystemd_dep,
                   opencv_dep,
                   avcodec_dep,
                   avformat_dep,
                   avutil_dep,
                   swscale_dep],
    install: true,
    install_dir: vrecmod_install_dir,
    install_rpath: sy_libdir
)
