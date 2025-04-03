# Syntalos IPC Helper

sy_roudi_src = ['main.cpp']

iceoryx_posh_roudi_dep = dependency('iceoryx_posh', version: '>= 2.0.5',
                                    modules: ['iceoryx_posh::iceoryx_posh_roudi'])

syntalos_roudi_exe = executable('syntalos-roudi',
    [sy_roudi_src],
    gnu_symbol_visibility: 'hidden',
    dependencies: [iceoryx_posh_dep,
                   iceoryx_posh_roudi_dep,
                   iceoryx_hoofs_dep,
                   syntalos_utils_dep,
    ],
    install: true,
    install_dir: sy_libdir,
    install_rpath: sy_libdir,
)
