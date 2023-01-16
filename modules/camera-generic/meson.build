# Build definitions for module: camera-generic

module_name = 'camera-generic'

module_hdr = [
    'genericcameramodule.h',
    'camera.h'
]
module_moc_hdr = [
    'genericcamerasettingsdialog.h'
]

module_src = [
    'camera.cpp',
    'genericcamerasettingsdialog.cpp'
]
module_moc_src = [
    'genericcameramodule.cpp'
]

module_ui = ['genericcamerasettingsdialog.ui']

v4l_dep = dependency('libv4l2')
module_deps = [
    opencv_dep,
    v4l_dep,
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
