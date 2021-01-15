# Syntalos Modules

module_lib_def_tmpl = files('module-lib-def.toml.in')

# Developer modules
subdir('devel.clock')
subdir('devel.datasource')
subdir('devel.datasst')
subdir('devel.pyooptest')

# Display / Aux modules
subdir('canvas')
subdir('table')

# Video
subdir('videorecorder')
subdir('videotransform')
subdir('camera-generic')
if get_option('mod-camera-tis')
    subdir('camera-tis')
endif
if get_option('mod-camera-flir')
    subdir('camera-flir')
endif
if get_option('mod-camera-ueye')
    subdir('camera-ueye')
endif
if get_option('mod-miniscope')
    subdir('miniscope')
endif
subdir('triled-tracker')

# Python & Customization
subdir('firmata-io')
subdir('firmata-userctl')
subdir('pyscript')

# Electrophysiology
subdir('rhd2000')
subdir('traceplot')

# Misc
subdir('runcmd')
