
# Developer modules
add_subdirectory(devel.clock)
add_subdirectory(devel.datasource)
add_subdirectory(devel.datasst)
add_subdirectory(devel.pyooptest)

# Display / Aux modules
add_subdirectory(canvas)
add_subdirectory(table)

# Video
add_subdirectory(videorecorder)
add_subdirectory(videotransform)
add_subdirectory(camera-generic)
if(MOD_CAMERA_TIS)
    add_subdirectory(camera-tis)
endif()
if(MOD_CAMERA_FLIR)
    add_subdirectory(camera-flir)
endif()
if(MOD_CAMERA_UEYE)
    add_subdirectory(camera-ueye)
endif()
if(MOD_MINISCOPE)
    add_subdirectory(miniscope)
endif()
add_subdirectory(triled-tracker)

# Python & Customization
add_subdirectory(firmata-io)
add_subdirectory(firmata-userctl)
add_subdirectory(pyscript)

# Electrophysiology
add_subdirectory(rhd2000)
add_subdirectory(traceplot)

# Misc
add_subdirectory(runcmd)


# registration
set(MA_MODULE_LIBS ${MA_MODULE_LIBS} PARENT_SCOPE)
set(MA_EXTRA_LIBS ${MA_EXTRA_LIBS} PARENT_SCOPE)
