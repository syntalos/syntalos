# Build definitions for module: miniscope

module_hdr = [
    'miniscopemodule.h',
]
module_moc_hdr = [
    'mscontrolwidget.h',
    'miniscopesettingsdialog.h',
]

module_src = [
    'miniscopesettingsdialog.cpp',
    'mscontrolwidget.cpp'
]
module_moc_src = [
    'miniscopemodule.cpp',
]

module_ui = ['miniscopesettingsdialog.ui']

miniscope_dep = dependency('miniscope', version: '>=0.5.0')
module_deps = [syntalos_fabric_dep,
               opencv_dep,
               vips_dep,
               miniscope_dep
]

module_data = [
    'miniscope.svg',
]

#
# Generic module setup
#
module_name = fs.name(meson.current_source_dir()).to_lower().underscorify().replace('_', '-')
mod_install_dir = join_paths(sy_modules_dir, fs.name(meson.current_source_dir()))

module_moc = qt.preprocess(
    moc_headers: module_moc_hdr,
    moc_sources: module_moc_src,
    ui_files: module_ui,
    qresources: module_res,
    dependencies: module_deps,
)
mod = shared_module(module_name,
    [module_hdr, module_moc_hdr,
     module_src, module_moc_src,
     module_moc],
    name_prefix: '',
    dependencies: [syntalos_fabric_dep,
                   module_deps],
    cpp_args: module_args,
    install: true,
    install_dir: mod_install_dir,
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
module_hdr = []
module_src = []
module_moc_hdr = []
module_moc_src = []
module_ui = []
module_res = []
module_deps = []
module_args = []
