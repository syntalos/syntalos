# Build definitions for module: devel.clock

module_name = 'devel.clock'

module_hdr = [
    'clockmodule.h'
]
module_moc_hdr = [
    'clocksettingsdialog.h'
]

module_src = [
    'clockmodule.cpp',
    'clocksettingsdialog.cpp'
]

module_ui = ['clocksettingsdialog.ui']

module_deps = [glib_dep]

#
# Generic module setup
#
module_moc = qt.preprocess(
    moc_headers: module_moc_hdr,
    ui_files: module_ui
)

mod_install_dir = join_paths(sy_modules_dir, fs.name(meson.current_source_dir()))

mod = shared_module(module_name,
    [module_hdr, module_moc_hdr,
     module_src, module_moc],
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
