# Build definitions for module: example-mlink

mod_install_dir = join_paths(sy_modules_dir, fs.name(meson.current_source_dir()))

example_mod_src = ['example-main.cpp']

executable('example-mlink',
    [example_mod_src],
    dependencies: [
        syntalos_mlink_dep,
        opencv_dep,
    ],
    install: true,
    install_dir: mod_install_dir,
)

fs.copyfile('module.toml', 'module.toml')
install_data('module.toml', install_dir: mod_install_dir)
