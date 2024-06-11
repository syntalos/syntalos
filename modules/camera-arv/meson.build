# Build definitions for module: camera-arv

aravis_dep = dependency('aravis-0.8', version: '>= 0.8.30')

module_hdr = [
    'araviscameramodule.h',
]

module_moc_hdr = [
    'qarv/qarvcameradelegate.h',
    'qarv/qarvcamera.h',
    'qarv/qarvdecoder.h',
    'qarv/qarvfeaturetree.h',
    'qarv/qarv-globals.h',
    'qarv/qarvtype.h',

    'qarv/decoders/bayer.h',
    'qarv/decoders/graymap.h',
    'qarv/decoders/mono12packed.h',
    'qarv/decoders/monounpackeddecoders.h',
    'qarv/decoders/monounpacked.h',
    'qarv/decoders/swscaledecoder.h',
    'qarv/decoders/unsupported.h',
    'qarv/decoders/bayer/BayerBG10.h',
    'qarv/decoders/bayer/BayerBG12.h',
    'qarv/decoders/bayer/BayerBG12_PACKED.h',
    'qarv/decoders/bayer/BayerBG16.h',
    'qarv/decoders/bayer/BayerBG8.h',
    'qarv/decoders/bayer/BayerGB10.h',
    'qarv/decoders/bayer/BayerGB12.h',
    'qarv/decoders/bayer/BayerGB12_PACKED.h',
    'qarv/decoders/bayer/BayerGB16.h',
    'qarv/decoders/bayer/BayerGB8.h',
    'qarv/decoders/bayer/BayerGR10.h',
    'qarv/decoders/bayer/BayerGR12.h',
    'qarv/decoders/bayer/BayerGR12_PACKED.h',
    'qarv/decoders/bayer/BayerGR16.h',
    'qarv/decoders/bayer/BayerGR8.h',
    'qarv/decoders/bayer/BayerRG10.h',
    'qarv/decoders/bayer/BayerRG12.h',
    'qarv/decoders/bayer/BayerRG12_PACKED.h',
    'qarv/decoders/bayer/BayerRG16.h',
    'qarv/decoders/bayer/BayerRG8.h',
    'qarv/decoders/bayer/decoder.h',

    'qarv/decoders/monounpackeddecoders/Mono10Format.h',
    'qarv/decoders/monounpackeddecoders/Mono12Format.h',
    'qarv/decoders/monounpackeddecoders/Mono14Format.h',
    'qarv/decoders/monounpackeddecoders/Mono16Format.h',
    'qarv/decoders/monounpackeddecoders/Mono8Format.h',
    'qarv/decoders/monounpackeddecoders/Mono8SignedFormat.h',

    'configwindow.h',
    'glvideowidget.h',
    'roicombobox.h',
]

module_src = [
    'qarv/qarv-globals.cpp',
    'qarv/qarvcamera.cpp',
    'qarv/qarvcameradelegate.cpp',
    'qarv/qarvdecoder.cpp',
    'qarv/qarvfeaturetree.cpp',
    'qarv/qarvtype.cpp',
    'qarv/decoders/bayer.cpp',
    'qarv/decoders/graymap.cpp',
    'qarv/decoders/mono12packed.cpp',
    'qarv/decoders/monounpackeddecoders.cpp',
    'qarv/decoders/swscaledecoder.cpp',

    'configwindow.cpp',
    'glvideowidget.cpp',
    'roicombobox.cpp',
]
module_moc_src = [
    'araviscameramodule.cpp',
]

module_ui = [
    'configwindow.ui',
]

module_res = []

module_deps = [
    aravis_dep,
    qt_net_dep,
    qt_svg_dep,
    qt_concurrent_dep,
    avformat_dep,
    swscale_dep,
    opencv_dep,
]

module_data = [
    'camera-arv.svg',
]

module_args = [
    '-DQT_STATICPLUGIN',
    '-fvisibility-inlines-hidden',
    #'-fvisibility=hidden',
    '-DARAVIS_HAVE_REGISTER_CACHE',
    '-DARAVIS_HAVE_08_API',
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
    qresources: module_res,
    dependencies: module_deps,
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
