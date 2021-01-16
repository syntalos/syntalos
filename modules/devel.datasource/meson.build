# Build definitions for module: devel.datasource

module_name = 'devel.datasource'

module_hdr = [
    'datasourcemodule.h'
]

module_src = [
    'datasourcemodule.cpp'
]

module_ui = []

module_deps = [opencv_dep]

#
# Generic module setup
#
module_moc = qt.preprocess(
    moc_sources: module_src,
    ui_files: module_ui
)

mod_install_dir = join_paths(sy_modules_dir, fs.name(meson.current_source_dir()))

mod = shared_module(module_name,
    [module_hdr, module_src, module_moc],
    name_prefix: '',
    dependencies: [syntalos_shared_dep,
                   module_deps],
    install: true,
    install_dir: mod_install_dir
)

mod_data = configuration_data()
mod_data.set('lib_name', fs.name(mod.full_path()))
configure_file(
    input: module_lib_def_tmpl,
    output: 'module.toml',
    configuration: mod_data,
    install: true,
    install_dir: mod_install_dir
)
