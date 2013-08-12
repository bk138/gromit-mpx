# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([gromit-mpx],[1.0],[Christian Beier <dontmind@freeshell.org>],[gromit-mpx],[https://github.com/bk138/gromit-mpx])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_SRCDIR([src/gromit-mpx.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_HEADER_STDC
PKG_PROG_PKG_CONFIG([0.20]) 

# Checks for libraries.
# check for gtk
PKG_CHECK_MODULES([GTK3], [gtk+-3.0 >= 2.99.3])
# check for XInput
PKG_CHECK_MODULES([XINPUT2], [xi >= 1.3])
# check for X
PKG_CHECK_MODULES([X11], [x11])
# check for math
AC_CHECK_LIB([m], [atan2])
# export linker flags in the right order
LIBS="$GTK3_LIBS $XINPUT2_LIBS $X11_LIBS $LIBS"

# ***************************************************************************
# finally
# ***************************************************************************
AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
