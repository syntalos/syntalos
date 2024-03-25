# Syntalos Shared and Binding Code

subdir('oop')

syntalos_fabric_src = [
    'cpuaffinity.h',
    'cpuaffinity.cpp',
    'edlstorage.h',
    'edlstorage.cpp',
    'eigenaux.h',
    'executils.h',
    'executils.cpp',
    'globalconfig.h',
    'globalconfig.cpp',
    'moduleapi.h',
    'moduleapi.cpp',
    'optionalwaitcondition.h',
    'optionalwaitcondition.cpp',
    'rtkit.h',
    'rtkit.cpp',
    'subscriptionwatcher.h',
    'subscriptionwatcher.cpp',
    'syclock.h',
    'syclock.cpp',
    'timesync.h',
    'timesync.cpp',
    'tsyncfile.h',
    'tsyncfile.cpp',

    'streams/atomicops.h',
    'streams/datatypes.h',
    'streams/datatypes.cpp',
    'streams/frametype.h',
    'streams/readerwriterqueue.h',
    'streams/stream.h',
    'streams/stream.cpp',

    'oop/oopmodule.h',
    'oop/oopworkerconnector.h',
    'oop/oopmodule.cpp',
    'oop/oopworkerconnector.cpp',
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
    moc_extra_arguments: ['--no-notes']
)

syntalos_fabric_lib = shared_library('syntalos-fabric',
    [syntalos_fabric_src,
     syntalos_fabric_moc,
     modconfig_h],
    link_with: sy_oopshared_lib,
    dependencies: [sy_base_deps,
                   syntalos_utils_dep,
                   opengl_dep,
                   vips_dep,
                   qt_dbus_dep,
                   qt_remoteobj_dep,

                   # still needed for QRO OOP
                   opencv_dep,
                   avutil_dep,
    ],
    include_directories: [root_include_dir,
                          syntalos_fabric_inc_dir,
                          include_directories('oop')],
    install: true,
    install_dir: sy_libdir
)

syntalos_fabric_dep = declare_dependency(
    link_with: syntalos_fabric_lib,
    dependencies: [sy_base_deps, syntalos_utils_dep],
    include_directories: [root_include_dir,
                          syntalos_fabric_inc_dir,
                          include_directories('oop')]
)
