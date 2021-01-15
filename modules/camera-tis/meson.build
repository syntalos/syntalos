# Build definitions for module: camera-tis

module_name = 'camera-tis'

tcam_dep = dependency('tcam')

module_hdr = []
module_moc_hdr = [
    'tiscameramodule.h',
    'tcamcamera.h',
    'cpropertiesdialog.h',
    'cdeviceselectiondlg.h'

]

module_src = []
module_moc_src = [
    'tiscameramodule.cpp',
    'tcamcamera.cpp',
    'cpropertiesdialog.cpp',
    'cdeviceselectiondlg.cpp'
]

module_ui = []

module_deps = [opencv_dep,
               tcam_dep,
               gstreamer_dep,
               gstreamer_app_dep,
               gstreamer_video_dep
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
