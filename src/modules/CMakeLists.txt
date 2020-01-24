
add_subdirectory(rhd2000)
add_subdirectory(traceplot)
add_subdirectory(videorecorder)
add_subdirectory(genericcamera)

if(MOD_CAMERA_UEYE)
    add_subdirectory(ueyecamera)
endif()

if(MOD_MINISCOPE)
    add_subdirectory(miniscope)
endif()

#add_subdirectory(triled-tracker)
add_subdirectory(firmata-io)
add_subdirectory(pyscript)
add_subdirectory(runcmd)
add_subdirectory(devel.datasource)
add_subdirectory(devel.pyooptest)

set(MA_MODULE_LIBS ${MA_MODULE_LIBS} PARENT_SCOPE)
set(MA_EXTRA_LIBS ${MA_EXTRA_LIBS} PARENT_SCOPE)
