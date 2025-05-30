# Build definitions for module: camera-flir

module_hdr = []
module_moc_hdr = [
    'flircameramod.h',
    'flircamera.h',
    'flircamsettingsdialog.h'

]

module_src = []
module_moc_src = [
    'flircameramod.cpp',
    'flircamera.cpp',
    'flircamsettingsdialog.cpp'
]

module_ui = ['flircamsettingsdialog.ui']

spinnaker_dep = cpp_compiler.find_library('Spinnaker', dirs: '/opt/spinnaker/lib')
spinnaker_cargs = []
found = false
foreach idir : ['/usr/include',
                '/usr/local/include',
                '/usr/include/spinnaker',
                '/usr/local/include/spinnaker',
                '/opt/spinnaker/include']
  inc_arg = '-isystem' + idir
  if cpp_compiler.has_header('Spinnaker.h', args: inc_arg)
    spinnaker_cargs += [inc_arg]
    break
  endif
endforeach
if spinnaker_cargs.length() == 0
    error('Unable to find Spinnaker SDK include directories! Please install the SDK or disable FLIR camera support.')
endif

module_deps = [opencv_dep, spinnaker_dep]

module_data = [
    'camera-flir.svg',
]

#
# Generic module setup
#
module_name = fs.name(meson.current_source_dir()).to_lower().underscorify().replace('_', '-')
mod_install_dir = join_paths(sy_modules_dir, fs.name(meson.current_source_dir()))

module_moc = qt.compile_moc(
    headers: module_moc_hdr,
    sources: module_moc_src,
    dependencies: module_deps,
    extra_args: ['--no-notes'],
)
module_moc += qt.compile_ui(sources: module_ui)
mod = shared_module(module_name,
    [module_hdr, module_moc_hdr,
     module_src, module_moc_src,
     module_moc],
    name_prefix: '',
    dependencies: [syntalos_fabric_dep,
                   module_deps],
    cpp_args: spinnaker_cargs,
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
module_hdr = []
module_src = []
module_moc_hdr = []
module_moc_src = []
module_ui = []
