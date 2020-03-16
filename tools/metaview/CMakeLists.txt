# CMakeLists for Syntalos MetaView

add_executable(syntalos-metaview
    main.cpp
    readtsync.h
    readtsync.cpp
)
target_link_libraries(syntalos-metaview
    ${SY_BASE_LIBS}
    syntalos_shared
)

include_directories(
    ${CMAKE_SOURCE_DIR}/src/
)

install(TARGETS syntalos-metaview DESTINATION ${CMAKE_INSTALL_BINDIR})
