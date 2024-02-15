# Syntalos Modules

module_lib_def_tmpl = files('module-lib-def.toml.in')
ln_exe = find_program('ln')
modules_enabled = get_option('modules')

# Developer modules
subdir('devel.clock')
subdir('devel.datasource')
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
subdir('sp210-pressuresense')

# Python & Customization
subdir('firmata-io')
subdir('firmata-userctl')
subdir('pyscript')

# Electrophysiology
if 'intan-rhx' in modules_enabled
    subdir('intan-rhx')
endif
subdir('traceplot')

# Misc
subdir('jsonwriter')
subdir('runcmd')
subdir('deeplabcut-live')

# Examples
subdir('example-py')
subdir('example-cpp')
