# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([gromit-mpx],[1.2],[Christian Beier <dontmind@freeshell.org>],[gromit-mpx],[https://github.com/bk138/gromit-mpx])
AM_INIT_AUTOMAKE([foreign subdir-objects -Wall])
AC_CONFIG_SRCDIR([src/gromit-mpx.c])
AC_CONFIG_HEADERS([build-config.h])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AX_CFLAGS_WARN_ALL
AC_HEADER_STDC
PKG_PROG_PKG_CONFIG([0.20]) 

# Checks for libraries.
# check for math
AC_SEARCH_LIBS([atan2], [m])
# check for gtk
PKG_CHECK_MODULES([GTK3], [gtk+-3.0 >= 2.99.3])
# libappindicator for tray icon
PKG_CHECK_MODULES([APPINDICATOR], appindicator3-0.1 >= 0.4.92)
# check for XInput
PKG_CHECK_MODULES([XINPUT2], [xi >= 1.3])
# check for X
PKG_CHECK_MODULES([X11], [x11])
# export linker flags in the right order
LIBS="$GTK3_LIBS $APPINDICATOR_LIBS $XINPUT2_LIBS $X11_LIBS $LIBS"

# ***************************************************************************
# finally
# ***************************************************************************
AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
