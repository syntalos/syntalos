# Syntalos Shared and Binding Code

syntalos_fabric_src = [
    'cvutils.h',
    'cvutils.cpp',
    'elidedlabel.h',
    'elidedlabel.cpp',
    'executils.h',
    'executils.cpp',
    'globalconfig.h',
    'globalconfig.cpp',
    'mlinkmodule.h',
    'mlinkmodule.cpp',
    'moduleapi.h',
    'moduleapi.cpp',
    'optionalwaitcondition.h',
    'optionalwaitcondition.cpp',
    'porteditordialog.h',
    'porteditordialog.cpp',
    'pyvenvmanager.h',
    'pyvenvmanager.cpp',
    'rangeslider.h',
    'rangeslider.cpp',
    'subscriptionwatcher.h',
    'subscriptionwatcher.cpp',
    'streamexporter.h',
    'streamexporter.cpp',
    'sysinfo.h',
    'sysinfo.cpp',
    'simpleterminal.h',
    'simpleterminal.cpp',

    'streams/atomicops.h',
    'streams/readerwriterqueue.h',
    'streams/stream.h',
    'streams/stream.cpp',
    'streams/subscriptionnotifier.h',
    'streams/subscriptionnotifier.cpp',

    # we reuse some shared primitives from mlink, to avoid linking
    # the mlink library into fabric
    '../mlink/ipc-iox-private.h',
    '../mlink/ipc-iox-private.cpp',
]

syntalos_fabric_ui = [
    'porteditordialog.ui',
]

syntalos_fabric_inc_dir = include_directories('.')

syntalos_fabric_h = []
syntalos_fabric_s = []
foreach s : syntalos_fabric_src
    if s.endswith('.h')
        syntalos_fabric_h += s
    elif s.endswith('.cpp')
        syntalos_fabric_s += s
    endif
endforeach

syntalos_fabric_moc = qt.compile_moc(
    headers: syntalos_fabric_h,
    sources: syntalos_fabric_s,
    extra_args: ['--no-notes']
)
syntalos_fabric_moc += qt.compile_ui(sources: syntalos_fabric_ui)

syntalos_fabric_lib = shared_library('syntalos-fabric',
    [syntalos_fabric_src,
     syntalos_fabric_moc,
     config_h,
     modconfig_h],
    dependencies: [
        rt_lib,
        thread_dep,
        eigen_dep,
        glib_dep,
        qt_dbus_dep,
        qt_gui_dep,
        qtermwidget_dep,
        syntalos_datactl_dep,
        syntalos_utils_dep,
        iox2_dep,
        opengl_dep,
        avutil_dep,
        opencv_dep,
    ],
    include_directories: [
        root_include_dir,
        syntalos_fabric_inc_dir
    ],
    install: true,
    install_dir: sy_libdir,
    install_rpath: sy_libdir,
)

syntalos_fabric_dep = declare_dependency(
    link_with: syntalos_fabric_lib,
    dependencies: [
        rt_lib,
        thread_dep,
        qt_gui_dep,
        syntalos_datactl_dep,
        syntalos_utils_dep,
    ],
    include_directories: [
        root_include_dir,
        syntalos_fabric_inc_dir
    ]
)
