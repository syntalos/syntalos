# Build definitions for module: camera-tis

tcam_dep = dependency('tcam', version: '>= 1.0')

module_hdr = [
    'device.h',
    'definitions.h',
    'caps.h',
]
module_moc_hdr = [
    'devicedialog.h',
    'devicewidget.h',
    'indexer.h',
    'propertiesbox.h',
    'propertyworker.h',
    'propertywidget.h',
    'tcamslider.h',
    'tcamcollection.h',
    'tcamspinbox.h',
    'tcamcontroldialog.h',
    'capswidget.h',
]

module_src = [
    'device.cpp',
    'devicedialog.cpp',
    'devicewidget.cpp',
    'indexer.cpp',
    'propertiesbox.cpp',
    'propertyworker.cpp',
    'propertywidget.cpp',
    'tcamslider.cpp',
    'tcamcollection.cpp',
    'tcamspinbox.cpp',
    'tcamcontroldialog.cpp',
    'caps.cpp',
    'capswidget.cpp',
]
module_moc_src = [
    'tiscameramodule.cpp',
]

module_ui = [
    'devicedialog.ui',
    'propertiesbox.ui',
    'tcamcontroldialog.ui',
]

module_src += qt.compile_resources(sources: ['tiscamera.qrc'])

module_deps = [tcam_dep,
               gstreamer_dep,
               gstreamer_app_dep,
               gstreamer_video_dep,
               opencv_dep,
]

module_data = [
    'camera-tis.svg',
]

#
# Generic module setup
#
module_name = fs.name(meson.current_source_dir()).to_lower().underscorify().replace('_', '-')
mod_install_dir = join_paths(sy_modules_dir, fs.name(meson.current_source_dir()))

module_moc = []
if module_moc_hdr.length() != 0 or module_moc_src.length() != 0
    module_moc += qt.compile_moc(
        headers: module_moc_hdr,
        sources: module_moc_src,
        dependencies: module_deps,
        extra_args: ['--no-notes'],
    )
endif
if module_ui.length() != 0
    module_moc += qt.compile_ui(sources: module_ui)
endif
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
module_deps = []
module_args = []
