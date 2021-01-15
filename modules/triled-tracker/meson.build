# Build definitions for module: triled-tracker

module_hdr = []
module_moc_hdr = [
    'triledtrackermodule.h',
    'tracker.h'
]

module_src = []
module_moc_src = [
    'triledtrackermodule.cpp',
    'tracker.cpp'
]

module_ui = []

module_deps = [opencv_dep]

#
# Generic module setup
#
module_name = fs.name(meson.current_source_dir()).to_lower().underscorify()
module_name = '-'.join(module_name.split('_'))
mod_install_dir = join_paths(sy_modules_dir, fs.name(meson.current_source_dir()))

module_moc = qt.preprocess(
    moc_headers: module_moc_hdr,
    moc_sources: module_moc_src,
    ui_files: module_ui
)
mod = shared_module(module_name,
    [module_hdr, module_moc_hdr,
     module_src, module_moc_src,
     module_moc],
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
