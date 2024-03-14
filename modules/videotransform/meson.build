# Build definitions for module: videotransform

module_hdr = [
    'videotransformmodule.h'
]
module_moc_hdr = [
    'videotransform.h',
    'vtransformlistmodel.h',
    'vtransformctldialog.h'
]

module_src = [
    'videotransform.cpp',
    'vtransformctldialog.cpp',
    'vtransformlistmodel.cpp'
]
module_moc_src = [
    'videotransformmodule.cpp'
]

module_ui = ['vtransformctldialog.ui']

module_deps = [vips_dep]

module_data = [
    'videotransform.svg',
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
install_data(
    module_data,
    install_dir: mod_install_dir,
    preserve_path: true
)
foreach fname : module_data
    fs.copyfile(fname)
endforeach
