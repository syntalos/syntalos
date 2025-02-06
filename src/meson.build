# Syntalos Framework

syntalos_main_inc_dir = include_directories('.')

subdir('utils')
subdir('datactl')
subdir('fabric')
subdir('roudi')
subdir('mlink')
subdir('python')

syntalos_src = [
    'aboutdialog.h',
    'aboutdialog.cpp',
    'appstyle.h',
    'appstyle.cpp',
    'commentdialog.h',
    'commentdialog.cpp',
    'entitylistmodels.h',
    'entitylistmodels.cpp',
    'engine.h',
    'engine.cpp',
    'flowgraphview.h',
    'flowgraphview.cpp',
    'globalconfigdialog.h',
    'globalconfigdialog.cpp',
    'intervalrundialog.h',
    'intervalrundialog.cpp',
    'main.cpp',
    'mainwindow.h',
    'mainwindow.cpp',
    'meminfo.h',
    'meminfo.cpp',
    'moduleeventthread.h',
    'moduleeventthread.cpp',
    'modulegraphform.h',
    'modulegraphform.cpp',
    'modulelibrary.h',
    'modulelibrary.cpp',
    'moduleselectdialog.h',
    'moduleselectdialog.cpp',
    'pymoduleloader.h',
    'pymoduleloader.cpp',
    'sysinfodialog.h',
    'sysinfodialog.cpp',
    'timingsdialog.h',
    'timingsdialog.cpp',
]

syntalos_ui = [
    'aboutdialog.ui',
    'commentdialog.ui',
    'globalconfigdialog.ui',
    'intervalrundialog.ui',
    'mainwindow.ui',
    'modulegraphform.ui',
    'moduleselectdialog.ui',
    'sysinfodialog.ui',
    'timingsdialog.ui'
]

syntalos_res = ['../data/resources.qrc']

syntalos_moc_h = []
syntalos_moc_s = []
foreach s : syntalos_src
    if s.endswith('.h')
        syntalos_moc_h += s
    elif s.endswith('.cpp')
        syntalos_moc_s += s
    endif
endforeach

syntalos_moc = qt.compile_moc(
    headers: syntalos_moc_h,
    sources: syntalos_moc_s,
    extra_args: ['--no-notes'],
)
syntalos_moc += qt.compile_ui(sources: syntalos_ui)
syntalos_moc += qt.compile_resources(sources: syntalos_res)

syntalos_exe = executable('syntalos',
    [syntalos_src, syntalos_moc, config_h, modconfig_h],
    gnu_symbol_visibility: 'hidden',
    dependencies: [syntalos_fabric_dep,
                   thread_dep,
                   qt_core_dep,
                   qt_gui_dep,
                   qt_opengl_dep,
                   qt_svg_dep,
                   qt_dbus_dep,
                   iceoryx_posh_dep,
                   iceoryx_hoofs_dep,

                   kfdbusaddons_dep,
                   kfarchive_dep,
                   kfconfigwidgets_dep,

                   libusb_dep,
                   avutil_dep,
                   eigen_dep,
                   glib_dep,
                   gobject_dep,
                   opengl_dep,
                   opencv_dep,
                   pipewire_dep,
                   gstreamer_dep,
                ],
    include_directories: [root_include_dir],
    install: true,
    install_rpath: sy_libdir
)
