# Syntalos Modules

# module setup boilerplate
module_lib_def_tmpl = files('module-lib-def.toml.in')
ln_exe = find_program('ln')
modules_enabled = get_option('modules')

module_hdr = []
module_src = []
module_moc_hdr = []
module_moc_src = []
module_ui = []
module_res = []
module_args = []

# Developer modules
subdir('devel.clock')
subdir('devel.datasource')
subdir('devel.latencytest')
subdir('devel.pyooptest')

# Display / Aux modules
subdir('canvas')
subdir('table')
if 'plot' in modules_enabled
    subdir('plot-timeseries')
endif

# Audio
subdir('audiosource')

# Video
subdir('videorecorder')
subdir('videotransform')
subdir('camera-generic')

if 'camera-arv' in modules_enabled
    subdir('camera-arv')
endif
if 'camera-tis' in modules_enabled
    subdir('camera-tis')
endif
if 'camera-flir' in modules_enabled
    subdir('camera-flir')
endif
if 'camera-ueye' in modules_enabled
    subdir('camera-ueye')
endif
if 'miniscope' in modules_enabled
    subdir('miniscope')
endif
subdir('triled-tracker')

# Device support
subdir('firmata-io')
subdir('firmata-userctl')
subdir('galdur-stim')
subdir('onix-commutator')
subdir('sp210-pressuresense')

# Scripting
subdir('pyscript')
subdir('cpp-workbench')
subdir('upy-workbench')

# Electrophysiology
if 'intan-rhx' in modules_enabled
    subdir('intan-rhx')
endif

# Misc
subdir('jsonwriter')
subdir('runcmd')
subdir('deeplabcut-live')

# Examples
subdir('example-py')
subdir('example-cpp')
