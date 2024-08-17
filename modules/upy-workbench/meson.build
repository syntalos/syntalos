# Build definitions for module: upy-workbench

module_hdr = [
    'upywbenchmodule.h'
]
module_moc_hdr = [
    'upyconsole.h',
]

module_src = [
    'upyconsole.cpp',
]
module_moc_src = [
    'upywbenchmodule.cpp'
]

module_ui = []

module_deps = [
    qt_serialport_dep,
    kftexteditor_dep,
]

module_data = [
    'upy-workbench.svg',
]

module_res = ['upywbres.qrc']

# Generate some documentation for the µPy Syntalos interface, if we can
pdoc_exe = find_program('pdoc', required: false)
if pdoc_exe.found()
    custom_target('upy-wbench-mkdoc',
                  build_by_default: true,
                  command : [python3_exe,
                             meson.current_source_dir() / 'make-docs.py',
                             '@OUTPUT0@'],
                  depend_files: ['make-docs.py', 'upy-comms.py'],
                  output: 'upy_sycomm_api_embed.html',
    )
endif

#
# Generic module setup
#
module_name = fs.name(meson.current_source_dir()).to_lower().underscorify().replace('_', '-')
mod_install_dir = join_paths(sy_modules_dir, fs.name(meson.current_source_dir()))

module_moc = qt.preprocess(
    moc_headers: module_moc_hdr,
    moc_sources: module_moc_src,
    ui_files: module_ui,
    qresources: module_res,
    dependencies: module_deps,
    moc_extra_arguments: ['--no-notes'],
)
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
module_res = []
module_deps = []
module_args = []
