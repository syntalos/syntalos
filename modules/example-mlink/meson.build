# Build definitions for module: example-mlink

mod_install_dir = join_paths(sy_modules_dir, fs.name(meson.current_source_dir()))

example_mod_src = ['example-main.cpp']
example_mod_data = [
    'module.toml',
    'example-mlink.svg',
]

executable('example-mlink',
    [example_mod_src],
    dependencies: [
        syntalos_mlink_dep,
        opencv_dep,
    ],
    install: true,
    install_dir: mod_install_dir,
)

foreach f : example_mod_data
    fs.copyfile(f)
endforeach
install_data(example_mod_data, install_dir: mod_install_dir)
