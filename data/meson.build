# Build definitions Syntalos static data

# .desktop file
install_data('syntalos.desktop',
    install_dir: join_paths(get_option('datadir'), 'applications')
)

# icon
install_data('syntalos.svg',
    install_dir: join_paths(get_option('datadir'), 'icons', 'hicolor', 'scalable', 'apps')
)

# metainfo file
metainfo_file = files('io.github.bothlab.syntalos.metainfo.xml')
ascli_exe = find_program('appstreamcli', required: false)
if ascli_exe.found()
  test('validate metainfo file',
        ascli_exe,
        args: ['validate',
               '--no-net',
               '--pedantic',
               metainfo_file]
  )
endif

install_data(metainfo_file,
    install_dir: join_paths(get_option ('datadir'), 'metainfo')
)
