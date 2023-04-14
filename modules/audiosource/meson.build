# Build definitions for module: audiosource

module_name = 'audiosource'
mod_install_dir = join_paths(sy_modules_dir, fs.name(meson.current_source_dir()))

module_hdr = [
    'audiosrcmodule.h',
]
module_moc_hdr = [
    'audiosettingsdialog.h',
]

module_src = [
    'audiosettingsdialog.cpp'
]
module_moc_src = [
    'audiosrcmodule.cpp'
]

module_ui = ['audiosettingsdialog.ui']

module_deps = [
    pipewire_dep,
    gstreamer_dep,
    gstreamer_app_dep,
    qt_svg_dep,
]

module_data = [
    'audiosource.svg',
]

#
# Generic module setup
#
module_moc = qt.preprocess(
    moc_headers: module_moc_hdr,
    moc_sources: module_moc_src,
    ui_files: module_ui
)

mod_install_dir = join_paths(sy_modules_dir, fs.name(meson.current_source_dir()))

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
)
foreach fname : module_data
    fs.copyfile(fname)
endforeach
