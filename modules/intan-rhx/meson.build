# Build definitions for module: intan-rhx

mod_install_dir = join_paths(sy_modules_dir, fs.name(meson.current_source_dir()))

subdir('Engine')
subdir('GUI')
subdir('udev')

#
# Sources
#
module_hdr = []
module_moc_hdr = [
    'intanrhxmodule.h',
    intanrhx_engine_hdr,
    intanrhx_gui_hdr,
    'chanexportdialog.h',
]

module_src = []
module_moc_src = [
    'intanrhxmodule.cpp',
    intanrhx_engine_src,
    intanrhx_gui_src,
    'chanexportdialog.cpp',
]

module_ui = []

module_res = ['IntanRHX.qrc']

# OpenCL kernel
configure_file(
    input: 'kernel.cl',
    output: 'kernel.cl',
    copy: true,
    install: true,
    install_dir: mod_install_dir
)

# FPGA bitfiles
intanrhx_bitfiles = [
    'FPGA-bitfiles/ConfigRHDController.bit',
    'FPGA-bitfiles/ConfigRHDInterfaceBoard.bit',
    'FPGA-bitfiles/ConfigRHSController.bit',
    'FPGA-bitfiles/ConfigXEM6010Tester.bit',
    'FPGA-bitfiles/USBEvaluationBoard.bit',
]
foreach bfile : intanrhx_bitfiles
    configure_file(
        input: bfile,
        output: fs.name(bfile),
        copy: true,
        install: true,
        install_dir: mod_install_dir
    )
endforeach

#
# Dependencies
#
qt_net_dep = dependency('qt5', modules: ['Network'])
qt_xml_dep = dependency('qt5', modules: ['Xml'])
okfp_lib = cpp_compiler.find_library(
    'okFrontPanel',
    dirs: join_paths(meson.current_source_dir(), 'libraries')
)
configure_file(
    input: 'libraries/libokFrontPanel.so',
    output: 'libokFrontPanel.so',
    copy: true,
    install: true,
    install_dir: mod_install_dir
)

if build_machine.cpu_family() != 'x86_64'
    error('Due to the proprietary OpalKelly FrontPanel in the Intan RHX module of Syntalos, ' +
          'it will only work on Intel/Amd x86 and x86_64 architectures. It looks like your CPU ' + build_machine.cpu() + 'is not supported.\n' +
          'You can compile Syntalos by disabling the `intan-rhx` module.')
endif

module_deps = [
    opengl_dep,
    opencl_dep,
    qt_net_dep,
    qt_xml_dep,
    okfp_lib,
    qt_multimedia_dep,
]

#
# Includes
#
module_inc_dirs = [intanrhx_engine_inc_dirs,
                   intanrhx_gui_inc_dirs,
                   include_directories('includes')]

# we don't want to fix all warnings in this code to keep it as close
# as possible to the pristine Intan code.
module_cpp_args = [
    '-Wno-error=suggest-override',
    '-Wno-suggest-override',
    '-Wno-non-virtual-dtor',
    '-Wno-odr'
]
if get_option('maintainer')
    module_cpp_args = ['-Wno-error=zero-as-null-pointer-constant',
                       '-Wno-error=deprecated-declarations',
                       '-Wno-error=unused-but-set-variable',
    ]
endif

# target OpenCL 3.0
module_cpp_args += ['-DCL_TARGET_OPENCL_VERSION=300']

#
# Generic module setup
#
module_name = fs.name(meson.current_source_dir()).to_lower().underscorify().replace('_', '-')
module_moc = qt.preprocess(
    moc_headers: module_moc_hdr,
    moc_sources: module_moc_src,
    ui_files: module_ui,
    qresources: module_res,
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
    include_directories: module_inc_dirs,
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
