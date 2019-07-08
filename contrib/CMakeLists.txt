 
# Intan Rhythm FPGA code blob
install(FILES main.bit DESTINATION "${CMAKE_INSTALL_DATADIR}/mazeamaze")

set (ARCH_X86 FALSE)
if (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)")
    set (X86 TRUE)
endif ()

set (ARCH_X86_64 FALSE)
if (CMAKE_SYSTEM_PROCESSOR MATCHES "(amd64)|(AMD64)|(x86_64)|(X86_64)")
    set (ARCH_X86_64 TRUE)
endif ()

if (NOT ${ARCH_X86_64} AND NOT ${ARCH_X86})
    message(FATAL_ERROR "Due to proprietary bits in the intan part of this software, it will only work on Intel/Amd x86 and x86_64 architectures. It looks like your architecture is not supported.")
endif()

if (${ARCH_X86_64})
    install(FILES "Opal Kelly library files/Linux 64-bit/libokFrontPanel.so" DESTINATION ${CMAKE_INSTALL_LIBDIR}/mazeamaze)
else ()
    if (${ARCH_X86})
        install(FILES "Opal Kelly library files/Linux 32-bit/libokFrontPanel.so" DESTINATION ${CMAKE_INSTALL_LIBDIR}/mazeamaze)
    endif ()
endif ()

# Install udev rules to make Opal Kelly USB device readable for normal users
install(FILES "udev/90-mazeamaze.rules" DESTINATION ${CMAKE_INSTALL_LIBDIR}/udev/rules.d/)
