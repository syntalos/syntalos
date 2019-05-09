
add_subdirectory(rhd2000)
add_subdirectory(traceplot)
add_subdirectory(videorecorder)
add_subdirectory(genericcamera)

if(MOD_CAMERA_UEYE)
    add_subdirectory(ueyecamera)
endif()
