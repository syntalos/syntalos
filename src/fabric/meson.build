# Syntalos Shared and Binding Code

syntalos_fabric_src = [
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
    'rangeslider.h',
    'rangeslider.cpp',
    'subscriptionwatcher.h',
    'subscriptionwatcher.cpp',
    'streamexporter.h',
    'streamexporter.cpp',
    'sysinfo.h',
    'sysinfo.cpp',

    'streams/atomicops.h',
    'streams/readerwriterqueue.h',
    'streams/stream.h',
    'streams/stream.cpp',
    'streams/subscriptionnotifier.h',
    'streams/subscriptionnotifier.cpp',
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

syntalos_fabric_moc = qt.preprocess(
    moc_headers: syntalos_fabric_h,
    moc_sources: syntalos_fabric_s,
    ui_files: syntalos_fabric_ui,
    moc_extra_arguments: ['--no-notes']
)

syntalos_fabric_lib = shared_library('syntalos-fabric',
    [syntalos_fabric_src,
     syntalos_fabric_moc,
     config_h,
     modconfig_h],
    dependencies: [sy_base_deps,
                   eigen_dep,
                   glib_dep,
                   qt_dbus_dep,
                   qt_gui_dep,
                   syntalos_datactl_dep,
                   syntalos_utils_dep,
                   opengl_dep,
                   avutil_dep,
                   opencv_dep,
                   iceoryx_posh_dep,
                   iceoryx_hoofs_dep,
    ],
    include_directories: [root_include_dir,
                          syntalos_fabric_inc_dir],
    install: true,
    install_dir: sy_libdir,
    install_rpath: sy_libdir,
)

syntalos_fabric_dep = declare_dependency(
    link_with: syntalos_fabric_lib,
    dependencies: [
        sy_base_deps,
        qt_gui_dep,
        syntalos_datactl_dep,
        syntalos_utils_dep
    ],
    include_directories: [root_include_dir,
                          syntalos_fabric_inc_dir]
)
