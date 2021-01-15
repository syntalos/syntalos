
subdir('oop')
subdir('pyworker')

syntalos_core_inc_dir = include_directories('.')

syntalos_shared_src = [
    'edlstorage.h',
    'edlstorage.cpp',
    'eigenaux.h',
    'moduleapi.h',
    'moduleapi.cpp',
    'optionalwaitcondition.h',
    'optionalwaitcondition.cpp',
    'rangeslider.h',
    'rangeslider.cpp',
    'subscriptionwatcher.h',
    'subscriptionwatcher.cpp',
    'syclock.h',
    'syclock.cpp',
    'timesync.h',
    'timesync.cpp',
    'tsyncfile.h',
    'tsyncfile.cpp',

    'utils/misc.h',
    'utils/misc.cpp',
    'utils/tomlutils.h',
    'utils/tomlutils.cpp',

    'streams/atomicops.h',
    'streams/datatypes.h',
    'streams/datatypes.cpp',
    'streams/frametype.h',
    'streams/readerwriterqueue.h',
    'streams/stream.h',
    'streams/stream.cpp'
]

syntalos_src = [
    'aboutdialog.h',
    'aboutdialog.cpp',
    'cpuaffinity.h',
    'cpuaffinity.cpp',
    'elidedlabel.h',
    'elidedlabel.cpp',
    'entitylistmodels.h',
    'entitylistmodels.cpp',
    'engine.h',
    'engine.cpp',
    'flowgraphview.h',
    'flowgraphview.cpp',
    'globalconfig.h',
    'globalconfig.cpp',
    'globalconfigdialog.h',
    'globalconfigdialog.cpp',
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
    'rtkit.h',
    'rtkit.cpp',
    'sysinfo.h',
    'sysinfo.cpp',
    'sysinfodialog.h',
    'sysinfodialog.cpp',
    'timingsdialog.h',
    'timingsdialog.cpp'
]

syntalos_ui = [
    'aboutdialog.ui',
    'globalconfigdialog.ui',
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
syntalos_shared_h = []
syntalos_shared_s = []
foreach s : syntalos_shared_src
    if s.endswith('.h')
        syntalos_shared_h += s
    elif s.endswith('.cpp')
        syntalos_shared_s += s
    endif
endforeach

syntalos_moc = qt.preprocess(
    moc_headers: syntalos_moc_h,
    moc_sources: syntalos_moc_s,
    ui_files: syntalos_ui,
    qresources: syntalos_res,
    moc_extra_arguments: ['--no-notes']
)

syntalos_shared_moc = qt.preprocess(
    moc_headers: syntalos_shared_h,
    moc_sources: syntalos_shared_s,
    moc_extra_arguments: ['--no-notes']
)

syntalos_shared_lib = static_library('syntalos_shared',
    [syntalos_shared_src, syntalos_shared_moc],
    name_prefix: '',
    dependencies: [sy_base_deps,
                   opengl_dep,
                   opencv_dep]
)

syntalos_shared_dep = declare_dependency(
    link_with: syntalos_shared_lib,
    dependencies: [sy_base_deps],
    include_directories: [include_directories('..'),
                          syntalos_core_inc_dir,
                          syntalos_oop_inc_dir]
)

syntalos_exe = executable('syntalos',
    [syntalos_src, syntalos_moc],
    gnu_symbol_visibility: 'hidden',
    dependencies: [syntalos_shared_dep,
                   thread_dep,
                   qt_core_dep,
                   qt_gui_dep,
                   qt_opengl_dep,
                   qt_svg_dep,
                   qt_dbus_dep,
                   qt_remoteobj_dep,

                   kfdbusaddons_dep,
                   kfarchive_dep,

                   avutil_dep,
                   eigen_dep,
                   glib_dep,
                   gobject_dep,
                   opengl_dep,
                   opencv_dep,
                   gstreamer_dep],
    link_with: [sy_oop_lib],
    include_directories: [include_directories('..')],
    install: true
)
