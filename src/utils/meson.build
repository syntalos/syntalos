# Syntalos Common Utilities

syntalos_utils_src = [
    'cpuaffinity.h',
    'cpuaffinity.cpp',
    'ipcconfig.h',
    'ipcconfig.cpp',
    'meminfo.h',
    'meminfo.cpp',
    'misc.h',
    'misc.cpp',
    'rtkit.h',
    'rtkit.cpp',
    'style.h',
    'style.cpp',
    'tomlutils.h',
    'tomlutils.cpp',
]

syntalos_utils_h = []
syntalos_utils_s = []
foreach s : syntalos_utils_src
    if s.endswith('.h')
        syntalos_utils_h += s
    elif s.endswith('.cpp')
        syntalos_utils_s += s
    endif
endforeach

syntalos_utils_moc = qt.compile_moc(
    headers: syntalos_utils_h,
    sources: syntalos_utils_s,
    extra_args: ['--no-notes']
)

syntalos_utils_lib = shared_library('syntalos-utils',
    [syntalos_utils_src,
     syntalos_utils_moc,
     config_h],
    dependencies: [sy_base_deps,
                   qt_dbus_dep,
                   qt_gui_dep,
                   opengl_dep,
    ],
    include_directories: [root_include_dir,
                          include_directories('.'),
                          syntalos_main_inc_dir],
    install: true,
    install_dir: sy_libdir,
)

syntalos_utils_dep = declare_dependency(
    link_with: syntalos_utils_lib,
    dependencies: [sy_base_deps],
    include_directories: [include_directories('.'),
                          syntalos_main_inc_dir
    ]
)
