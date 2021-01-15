# Build definitions for module: camera-flir

module_name = 'camera-flir'

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

spinnaker_dep = find_library('Spinnaker')
spinnaker_cargs = []
found = false
foreach idir : ['/usr/include',
                '/usr/local/include',
                '/usr/include/spinnaker',
                '/usr/local/include/spinnaker']
  inc_arg = '-I' + idir
  if cpp_compiler.has_header('Spinnaker.h', args: inc_arg)
    spinnaker_cargs += [inc_arg]
    break
  endif
endforeach
if not spinnaker_cargs
    error('Unable to find Spinnaker SDK include directories! Please install the SDK or disable FLIR camera support.')
endif

module_deps = [opencv_dep, spinnaker_dep]

#
# Module setup
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
