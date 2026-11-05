# Unit tests for the ONNX inference module

# The test models are generated with the onnx Python package. Without it, the
# tests that need a model are skipped.
onnxinfer_test_models = []
py_onnx = import('python').find_installation('python3', modules: ['onnx'], required: false)
onnxinfer_have_test_models = py_onnx.found()
if onnxinfer_have_test_models
    onnxinfer_test_models = custom_target('onnxinfer-test-models',
        output: [
            'identity-image.onnx',
            'identity-u8-nhwc.onnx',
            'channel-mean.onnx',
        ],
        command: [py_onnx, files('make-onnx-test-models.py'), '@OUTDIR@'],
        build_by_default: true,
    )
    # used by the scenario test in tests/
    onnxinfer_scenario_model = onnxinfer_test_models[2]
else
    warning('Python module "onnx" not found, ONNX inference tests that need a model will be skipped.')
endif

test_onnxinfer_moc = qt.compile_moc(
    sources: ['test-onnxinfer.cpp'],
    dependencies: [qt_test_dep],
)

test_onnxinfer_exe = executable('test-onnxinfer',
    ['test-onnxinfer.cpp', test_onnxinfer_moc],
    dependencies: [onnxinfer_core_dep, qt_test_dep],
    cpp_args: ['-DONNXINFER_TEST_MODEL_DIR="@0@"'.format(meson.current_build_dir())],
)

test('sy-test-onnxinfer',
    test_onnxinfer_exe,
    depends: onnxinfer_test_models,
)
