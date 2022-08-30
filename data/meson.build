# Build definitions Syntalos static data

# desktop-entry file
install_data('syntalos.desktop',
    install_dir: join_paths(get_option('datadir'), 'applications')
)

# icon
install_data('syntalos.svg',
    install_dir: join_paths(get_option('datadir'), 'icons', 'hicolor', 'scalable', 'apps')
)

# systemd tmpfilesd snippet (for systemd which don't clean this up on their own)
if systemd_dep.found()
    install_data('tmpfiles.d/syntalos.conf',
        install_dir: systemd_dep.get_pkgconfig_variable('tmpfiles_dir')
    )
endif

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

    metainfo_with_rel = custom_target('gen-metainfo-rel',
        input: ['../NEWS.md', metainfo_file],
        output: ['io.github.bothlab.syntalos.metainfo.xml'],
        command: [ascli_exe, 'news-to-metainfo',
                  '--format=text', '--limit=6',
                  '@INPUT0@', '@INPUT1@', '@OUTPUT@'],
        install: true,
        install_dir: join_paths(get_option ('datadir'), 'metainfo')
    )
else
    install_data(metainfo_file,
        install_dir: join_paths(get_option ('datadir'), 'metainfo')
    )
endif
