# Build definitions for module: pyscript

module_hdr = [
    'pyscriptmodule.h'
]
module_moc_hdr = [
    'porteditordialog.h'
]

module_src = [
    'porteditordialog.cpp'
]
module_moc_src = [
    'pyscriptmodule.cpp'
]

module_ui = ['porteditordialog.ui']

kftexteditor_dep = dependency('KF5TextEditor')
module_deps = [kftexteditor_dep, qt_remoteobj_dep]

module_data = []

#
# Generic module setup
#
module_name = fs.name(meson.current_source_dir()).to_lower().underscorify().replace('_', '-')
mod_install_dir = join_paths(sy_modules_dir, fs.name(meson.current_source_dir()))

module_moc = qt.preprocess(
    moc_headers: module_moc_hdr,
    moc_sources: module_moc_src,
    ui_files: module_ui,
)
mod = shared_module(module_name,
    [module_hdr, module_moc_hdr,
     module_src, module_moc_src,
     module_moc],
    name_prefix: '',
    dependencies: [syntalos_fabric_dep,
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
install_data(
    module_data,
    install_dir: mod_install_dir,
    preserve_path: true
)
foreach fname : module_data
    fs.copyfile(fname)
endforeach
