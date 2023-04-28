# Build definitions for module: videorecorder

vrecmod_install_dir = join_paths(sy_modules_dir, fs.name(meson.current_source_dir()))

# small helper tool to enqueue files for deferred encoding
subdir('encodehelper')

module_hdr = [
    'videorecordmodule.h',
    'videowriter.h'
]
module_moc_hdr = [
    'recordersettingsdialog.h',
]

module_src = [
    'videowriter.cpp',
    'recordersettingsdialog.cpp'
]
module_moc_src = [
    'videorecordmodule.cpp'
]

module_ui = ['recordersettingsdialog.ui']

module_deps = [libsystemd_dep,
               opencv_dep,
               avcodec_dep,
               avformat_dep,
               avutil_dep,
               swscale_dep,
               qt_dbus_dep,
]

module_data = [
    'videorecorder.svg',
]

#
# Generic module setup
#
module_name = fs.name(meson.current_source_dir()).to_lower().underscorify().replace('_', '-')
mod_install_dir = vrecmod_install_dir

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
