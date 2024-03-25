# Syntalos Common UI Components

syntalos_uilib_src = [
    'elidedlabel.h',
    'elidedlabel.cpp',
    'rangeslider.h',
    'rangeslider.cpp',
]

syntalos_uilib_h = []
syntalos_uilib_s = []
foreach s : syntalos_uilib_src
    if s.endswith('.h')
        syntalos_uilib_h += s
    elif s.endswith('.cpp')
        syntalos_uilib_s += s
    endif
endforeach

syntalos_uilib_moc = qt.preprocess(
    moc_headers: syntalos_uilib_h,
    moc_sources: syntalos_uilib_s,
    moc_extra_arguments: ['--no-notes']
)

syntalos_uilib_lib = static_library('syntalos-uilib-private',
    [syntalos_uilib_src,
     syntalos_uilib_moc],
    dependencies: [sy_base_deps],
    include_directories: [include_directories('.')],
    install: false
)

syntalos_uilib_dep = declare_dependency(
    link_with: syntalos_uilib_lib,
    dependencies: [sy_base_deps],
    include_directories: [include_directories('.')]
)
