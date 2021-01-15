# Build definitions for module: videorecorder

module_name = 'videorecorder'

module_hdr = [
    'videorecordmodule.h',
    'videowriter.h',
    'recordersettingsdialog.h',

]

module_src = [
    'videorecordmodule.cpp',
    'videowriter.cpp',
    'recordersettingsdialog.cpp'
]

module_ui = ['recordersettingsdialog.ui']

module_deps = [opencv_dep,
               avcodec_dep,
               avformat_dep,
               avutil_dep,
               swscale_dep
]

#
# Generic module setup
#
module_moc = qt.preprocess(
    moc_headers: module_hdr,
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
