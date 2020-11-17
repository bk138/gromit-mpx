cmake_minimum_required(VERSION 3.0)
project(gromit-mpx LANGUAGES C)
include(GNUInstallDirs)

set(target_name gromit-mpx)
set(version 1.4)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(CMAKE_GENERATOR MATCHES "Unix Makefiles|Ninja")
# some LSP servers expect compile_commands.json in the project root
add_custom_target(
    copy-compile-commands ALL
    ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif(CMAKE_GENERATOR MATCHES "Unix Makefiles|Ninja")

set(CMAKE_C_FLAGS  " ${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter")

configure_file(build-config.h_cmake_in build-config.h)

find_package(PkgConfig)

pkg_check_modules(gtk3 REQUIRED "gtk+-3.0 >= 3.22")
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
    src/paint_cursor.xpm
    src/erase_cursor.xpm
)

add_executable(${target_name} ${sources})

target_link_libraries(${target_name}
    ${gtk3_LIBRARIES}
    ${appindicator3_LIBRARIES}
    ${xinput_LIBRARIES}
    ${x11_LIBRARIES}
    -lm
)


install(TARGETS ${target_name} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/net.christianbeier.Gromit-MPX.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES data/gromit-mpx.cfg DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/gromit-mpx)
install(FILES README.md AUTHORS ChangeLog NEWS.md DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(FILES gromit-mpx.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
install(FILES data/gromit-mpx.png data/gromit-mpx_active.png data/gromit-mpx.xpm DESTINATION ${CMAKE_INSTALL_DATADIR}/pixmaps)
install(FILES data/net.christianbeier.Gromit-MPX.svg data/gromit-mpx.svg data/gromit-mpx_active.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
install(FILES data/net.christianbeier.Gromit-MPX.appdata.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
