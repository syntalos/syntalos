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
    'FPGA-bitfiles/ConfigRHDController_7310.bit',
    'FPGA-bitfiles/ConfigRHDController.bit',
    'FPGA-bitfiles/ConfigRHDInterfaceBoard.bit',
    'FPGA-bitfiles/ConfigRHSController_7310.bit',
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

# We should use find_library() but can't do that currently because
# of Meson bug #7766:
# https://github.com/mesonbuild/meson/issues/7766
#okfp_lib = cpp_compiler.find_library(
#    'okFrontPanel',
#    dirs: join_paths(meson.current_source_dir(), 'libraries'),
#    static: false
#)
okfp_lib_dir = join_paths(meson.current_source_dir(), 'libraries')
okfp_lib = declare_dependency(link_args: ['-L' + okfp_lib_dir, '-lokFrontPanel'])

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
# FIXME! (but upstream)
module_cpp_args = [
    '-Wno-error=suggest-override',
    '-Wno-suggest-override',
    '-Wno-non-virtual-dtor',

    '-Wno-error=type-limits', # this shows real errors, but we rather want to fix them upstream @ Intan
    '-Wno-type-limits',

    '-Wno-error=unused-but-set-variable',
    '-Wno-unused-but-set-variable',

    '-Wno-error=deprecated-declarations',
    '-Wno-deprecated-declarations',
]
if get_option('maintainer')
    module_cpp_args += ['-Wno-error=zero-as-null-pointer-constant',
                        '-Wno-error=non-virtual-dtor',
                        '-Wno-error=sequence-point',
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
    install_rpath: mod_install_dir,
    build_rpath: okfp_lib_dir,
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
