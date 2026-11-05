# Build definitions for module: onnx-inference

mod_install_dir = join_paths(sy_modules_dir, fs.name(meson.current_source_dir()))

# Debian ships a pkg-config file, ONNX Runtime's own builds only a CMake package.
# Its headers are not -Wshadow clean, hence the system include type.
ort_dep = dependency('libonnxruntime', 'onnxruntime', version: '>= 1.21', include_type: 'system')

#
# Core library: engine, recipe, adapters and decoders.
# It has no Qt and no Syntalos dependency, so the unit tests can use it directly.
#
onnxinfer_core_hdr = [
    'tensor.h',
    'params.h',
    'units.h',
    'recipe.h',
    'ortengine.h',
    'adapters/imageadapter.h',
    'decoders/decoder.h',
    'decoders/rawdecoder.h',
    'decoders/scoresdecoder.h',
]
onnxinfer_core_src = [
    'tensor.cpp',
    'params.cpp',
    'units.cpp',
    'recipe.cpp',
    'ortengine.cpp',
    'adapters/imageadapter.cpp',
    'decoders/decoder.cpp',
    'decoders/rawdecoder.cpp',
    'decoders/scoresdecoder.cpp',
]
onnxinfer_core_lib = static_library('onnxinfer-core',
    [onnxinfer_core_src, onnxinfer_core_hdr],
    dependencies: [ort_dep, opencv_dep, toml_dep],
    gnu_symbol_visibility: 'hidden',
)
onnxinfer_core_dep = declare_dependency(
    link_with: onnxinfer_core_lib,
    include_directories: include_directories('.'),
    dependencies: [ort_dep, opencv_dep, toml_dep],
)

#
# The module executable
#
onnxinfer_moc_hdr = [
    'onnxinfermodule.h',
    'settingsdialog.h',
    'paramform.h',
]
onnxinfer_src = [
    'onnxinfer-main.cpp',
    'onnxinfermodule.cpp',
    'settingsdialog.cpp',
    'paramform.cpp',
]
onnxinfer_moc = qt.compile_moc(
    headers: onnxinfer_moc_hdr,
    dependencies: [qt_gui_dep],
    extra_args: ['--no-notes'],
)

executable('onnx-inference',
    [onnxinfer_src, onnxinfer_moc_hdr, onnxinfer_moc],
    gnu_symbol_visibility: 'hidden',
    dependencies: [
        onnxinfer_core_dep,
        syntalos_mlink_dep,
        qt_core_dep,
        qt_gui_dep,
        qt_concurrent_dep,
    ],
    install: true,
    install_dir: mod_install_dir,
    install_rpath: sy_libdir,
)

onnxinfer_data = [
    'module.toml',
    'onnx-inference.svg',
]
foreach f : onnxinfer_data
    fs.copyfile(f)
endforeach
install_data(onnxinfer_data, install_dir: mod_install_dir)

subdir('tests')
