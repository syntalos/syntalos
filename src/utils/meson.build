# Syntalos Common Utilities

syntalos_utils_src = [
    'misc.h',
    'misc.cpp',
    'style.h',
    'style.cpp',
    'tomlutils.h',
    'tomlutils.cpp',
    'vipsutils.h',
    'vipsutils.cpp',
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

syntalos_utils_moc = qt.preprocess(
    moc_headers: syntalos_utils_h,
    moc_sources: syntalos_utils_s,
    moc_extra_arguments: ['--no-notes']
)

syntalos_utils_lib = shared_library('syntalos-utils',
    [syntalos_utils_src,
     syntalos_utils_moc],
    dependencies: [sy_base_deps,
                   opencv_dep,
                   opengl_dep,
                   vips_dep],
    include_directories: [root_include_dir,
                          include_directories('.'),
                          syntalos_main_inc_dir],
    install: true,
    install_dir: sy_libdir
)

syntalos_utils_dep = declare_dependency(
    link_with: syntalos_utils_lib,
    dependencies: [sy_base_deps],
    include_directories: [include_directories('.'),
                          syntalos_main_inc_dir
    ]
)
