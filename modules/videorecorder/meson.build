# Build definitions for module: videorecorder

vrecmod_install_dir = join_paths(sy_modules_dir, fs.name(meson.current_source_dir()))

# small helper tool to enqueue files for deferred encoding
subdir('encodehelper')

module_hdr = [
    'videorecordmodule.h',
    'videowriter.h',
    'ffmpeg-utils.h',
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
mod_install_dir = join_paths(sy_modules_dir, fs.name(meson.current_source_dir()))

module_moc = []
module_hdr_moc = []
module_src_moc = []
module_ui_moc = []
if module_moc_hdr.length() != 0
    module_hdr_moc = qt.compile_moc(
        headers: module_moc_hdr,
        dependencies: module_deps,
        extra_args: ['--no-notes'],
    )
    module_moc += module_hdr_moc
endif
if module_moc_src.length() != 0
    module_src_moc = qt.compile_moc(
        sources: module_moc_src,
        dependencies: module_deps,
        extra_args: ['--no-notes'],
    )
    module_moc += module_src_moc
endif
if module_ui.length() != 0
    module_ui_moc = qt.compile_ui(sources: module_ui)
    module_moc += module_ui_moc
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
    install_rpath: sy_libdir,
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

vrec_settings_test_src = ['recordersettingsdialog-test.cpp']
vrec_settings_test_moc = qt.compile_moc(sources: vrec_settings_test_src)
vrec_settings_test_exe = executable(
    'test-videorecorder-settings',
    [
        vrec_settings_test_src,
        'videowriter.cpp',
        'recordersettingsdialog.cpp',
        module_hdr_moc,
        module_ui_moc,
        vrec_settings_test_moc,
    ],
    dependencies: [
        syntalos_fabric_dep,
        module_deps,
        qt_test_dep,
    ],
    cpp_args: module_args,
)
vrec_settings_test_env = environment()
vrec_settings_test_env.set('QT_QPA_PLATFORM', 'offscreen')
test(
    'sy-test-videorecorder-settings',
    vrec_settings_test_exe,
    env: vrec_settings_test_env,
    is_parallel: true,
)

module_hdr = []
module_src = []
module_moc_hdr = []
module_moc_src = []
module_ui = []
module_deps = []
module_args = []
