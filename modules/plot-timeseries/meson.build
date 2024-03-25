# Build definitions for module: plot-timeseries

module_hdr = [
    'plotseriesmodule.h',
    'qtimgui.h',
]
module_moc_hdr = [
    'imgui-renderer.h',
    'timeplotwidget.h',
    'plotwindow.h',
]

module_src = [
    'imgui-renderer.cpp',
    'qtimgui.cpp',
    'timeplotwidget.cpp',
    'plotwindow.cpp',
]
module_moc_src = [
    'plotseriesmodule.cpp',
]

module_ui = [
    'plotwindow.ui',
]

#imgui_dep = dependency('imgui', version: '>= 1.86',
#                       fallback: ['imgui', 'imgui_dep'])

module_deps = [imgui_dep, implot_dep]

module_data = [
    'plot-timeseries.svg',
]

#
# Generic module setup
#
module_name = fs.name(meson.current_source_dir()).to_lower().underscorify().replace('_', '-')
mod_install_dir = join_paths(sy_modules_dir, fs.name(meson.current_source_dir()))

module_moc = qt.preprocess(
    moc_headers: module_moc_hdr,
    moc_sources: module_moc_src,
    ui_files: module_ui,
)
mod = shared_module(module_name,
    [module_hdr, module_moc_hdr,
     module_src, module_moc_src,
     module_moc],
    name_prefix: '',
    dependencies: [syntalos_fabric_dep,
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
install_data(
    module_data,
    install_dir: mod_install_dir,
    preserve_path: true
)
foreach fname : module_data
    fs.copyfile(fname)
endforeach
