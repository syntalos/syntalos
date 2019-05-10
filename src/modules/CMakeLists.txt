
add_subdirectory(rhd2000)
add_subdirectory(traceplot)
add_subdirectory(videorecorder)
add_subdirectory(genericcamera)

if(MOD_CAMERA_UEYE)
    add_subdirectory(ueyecamera)
endif()

add_subdirectory(triled-tracker)
add_subdirectory(firmata-io)

set(MA_MODULE_LIBS ${MA_MODULE_LIBS} PARENT_SCOPE)
