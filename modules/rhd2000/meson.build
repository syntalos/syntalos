# Build definitions for module: rhd2000

module_name = fs.name(meson.current_source_dir()).to_lower().underscorify().replace('_', '-')
mod_install_dir = join_paths(sy_modules_dir, fs.name(meson.current_source_dir()))
rhd2000_mod_install_dir = mod_install_dir

subdir('contrib')

module_hdr = []
module_moc_hdr = [
    'rhd2000module.h',
    'auxdigoutconfigdialog.h',
    'bandwidthdialog.h',
    'cabledelaydialog.h',
    'globalconstants.h',
    'helpdialogchipfilters.h',
    'helpdialogcomparators.h',
    'helpdialogdacs.h',
    'helpdialogfastsettle.h',
    'helpdialoghighpassfilter.h',
    'helpdialognotchfilter.h',
    'impedancefreqdialog.h',
    'intanui.h',
    'keyboardshortcutdialog.h',
    'okFrontPanelDLL.h',
    'qtincludes.h',
    'randomnumber.h',
    'renamechanneldialog.h',
    'rhd2000datablock.h',
    'rhd2000evalboard.h',
    'rhd2000registers.h',
    'setsaveformatdialog.h',
    'signalchannel.h',
    'signalgroup.h',
    'signalprocessor.h',
    'signalsources.h',
    'spikeplot.h',
    'spikescopedialog.h',
    'triggerrecorddialog.h',
    'waveplot.h',
]

module_src = []
module_moc_src = [
    'rhd2000module.cpp',
    'auxdigoutconfigdialog.cpp',
    'bandwidthdialog.cpp',
    'cabledelaydialog.cpp',
    'helpdialogchipfilters.cpp',
    'helpdialogcomparators.cpp',
    'helpdialogdacs.cpp',
    'helpdialogfastsettle.cpp',
    'helpdialoghighpassfilter.cpp',
    'helpdialognotchfilter.cpp',
    'impedancefreqdialog.cpp',
    'intanui.cpp',
    'keyboardshortcutdialog.cpp',
    'okFrontPanelDLL.cpp',
    'randomnumber.cpp',
    'renamechanneldialog.cpp',
    'rhd2000datablock.cpp',
    'rhd2000evalboard.cpp',
    'rhd2000registers.cpp',
    'setsaveformatdialog.cpp',
    'signalchannel.cpp',
    'signalgroup.cpp',
    'signalprocessor.cpp',
    'signalsources.cpp',
    'spikeplot.cpp',
    'spikescopedialog.cpp',
    'triggerrecorddialog.cpp',
    'waveplot.cpp'
]

module_ui = []

module_deps = []

intan_conf = configuration_data()
intan_conf.set_quoted('INTAN_MODULE_DIR', rhd2000_mod_install_dir)
intan_conf.set_quoted('RHYTHM_BITFILE_NAME', rhythm_bitfile_fname)
intan_conf.set_quoted('OK_LIB_NAME', oklib_install_fullpath)
configure_file(output: 'config-rhd2000.h', configuration: intan_conf)

# we don't want to fix all warnings in this code to keep it as close
# as possible to the pristine Intan code.
module_cpp_args = []
if get_option('maintainer')
    module_cpp_args = ['-Wno-error=zero-as-null-pointer-constant',
                       '-Wno-error=deprecated-declarations',
                       '-Wno-error=unused-but-set-variable'
    ]
endif

#
# Module setup
#
module_moc = qt.preprocess(
    moc_headers: module_moc_hdr,
    moc_sources: module_moc_src,
    ui_files: module_ui,
    moc_extra_arguments: ['--no-notes']
)
mod = shared_module(module_name,
    [module_hdr, module_moc_hdr,
     module_src, module_moc_src,
     module_moc],
    name_prefix: '',
    dependencies: [syntalos_shared_dep,
                   module_deps],
    install: true,
    install_dir: mod_install_dir,
    cpp_args: module_cpp_args
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
