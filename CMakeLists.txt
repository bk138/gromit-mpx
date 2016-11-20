cmake_minimum_required(VERSION 3.0)
project(gromit-mpx LANGUAGES C)

set(target_name gromit-mpx)
set(version 1.3)

set(CMAKE_C_FLAGS  " ${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter")

configure_file(build-config.h_cmake_in build-config.h)

find_package(PkgConfig)

pkg_check_modules(gtk3 REQUIRED "gtk+-3.0 >= 2.99.3")
pkg_check_modules(appindicator3 REQUIRED "appindicator3-0.1 >= 0.4.92")
pkg_check_modules(xinput REQUIRED "xi >= 1.3")
pkg_check_modules(x11 REQUIRED x11)

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${gtk3_INCLUDE_DIRS}
    ${appindicator3_INCLUDE_DIRS}
    ${xinput_INCLUDE_DIRS}
    ${x11_INCLUDE_DIRS}
)

link_directories(
    ${gtk3_LIBRARY_DIRS}
    ${appindicator3_LIBRARY_DIRS}
    ${xinput_LIBRARY_DIRS}
    ${x11_LIBRARY_DIRS}
)

set(sources
    src/callbacks.c
    src/callbacks.h
    src/config.c
    src/config.h
    src/gromit-mpx.c
    src/gromit-mpx.h
    src/input.c
    src/input.h
)

add_executable(${target_name} ${sources})

target_link_libraries(${target_name}
    ${gtk3_LIBRARIES}
    ${appindicator3_LIBRARIES}
    ${xinput_LIBRARIES}
    ${x11_LIBRARIES}
    -lm
)

include(GNUInstallDirs)

install(TARGETS ${target_name} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/gromit-mpx.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES data/gromit-mpx.cfg DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/gromit-mpx)
install(FILES README.md AUTHORS ChangeLog NEWS DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(FILES gromit-mpx.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
install(FILES data/gromit-mpx.png data/gromit-mpx.xpm DESTINATION ${CMAKE_INSTALL_DATADIR}/pixmaps)
install(FILES data/gromit-mpx.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
