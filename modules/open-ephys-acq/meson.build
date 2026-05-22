# Build definitions for module: open-ephys-acq

module_hdr = [
    'oeacqmodule.h',
    'devices/AcquisitionBoard.h',
    'devices/Headstage.h',
    'devices/ImpedanceMeter.h',
    'devices/simulated/AcqBoardSim.h',
    'devices/simulated/HeadstageSim.h',
    'devices/simulated/ImpedanceMeterSim.h',
    'devices/simulated/SimulatedData.h',
    'devices/oni/AcqBoardONI.h',
    'devices/oni/HeadstageONI.h',
    'devices/oni/ImpedanceMeterONI.h',
]
module_moc_hdr = [
    'oeacqsettingsdialog.h',
    'oeacqimpedancedialog.h',
]

module_src = [
    'devices/simulated/AcqBoardSim.cpp',
    'devices/simulated/HeadstageSim.cpp',
    'devices/simulated/ImpedanceMeterSim.cpp',
    'devices/oni/AcqBoardONI.cpp',
    'devices/oni/HeadstageONI.cpp',
    'devices/oni/ImpedanceMeterONI.cpp',
    'devices/oni/rhythm-api/rhd2000ONIboard.cpp',
    'devices/oni/rhythm-api/rhd2000ONIdatablock.cpp',
    'devices/oni/rhythm-api/rhd2000ONIregisters.cpp',
]
module_moc_src = [
    'oeacqmodule.cpp',
    'oeacqsettingsdialog.cpp',
    'oeacqimpedancedialog.cpp',
]

module_ui = []

module_deps = [
    liboni_dep,
]

module_data = [
    'open-ephys-acq.svg',
]

# The Open Ephys rhythm-api vendor sources under devices/oni/rhythm-api/ trip a
# handful of warnings (sign-compare, enum-enum arithmetic, the occasional
# unused variable) that Syntalos otherwise treats as errors. Vendor code is
# kept as-is, so we relax these specific diagnostics for this module only.
module_args = [
    '-Wno-error=sign-compare',
    '-Wno-error=deprecated-enum-enum-conversion',
    '-Wno-error=unused-variable',
    '-Wno-error=unused-but-set-variable',
]

subdir('udev')

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
module_hdr = []
module_src = []
module_moc_hdr = []
module_moc_src = []
module_ui = []
module_deps = []
module_args = []
