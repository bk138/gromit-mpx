# Process this file with autoconf to produce a configure script.

AC_INIT(gromit-mpx, 0.1, http://dontmind.freeshell.org)
AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)
AM_CONFIG_HEADER(config.h)


# Check if we have enabled debug support.
AC_MSG_CHECKING(whether to enable debugging)
AC_ARG_ENABLE(debug, [  --enable-debug          enable debug build [[default=no]]],, enable_debug="no")
if test "x$enable_debug" = "xyes"; then
	CPPFLAGS="$CPPFLAGS -Wall -Wextra -Wno-unused-parameter -DDEBUG"
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi

AC_ISC_POSIX
AC_PROG_CPP
AC_PROG_CC
AM_PROG_CC_STDC
AM_PROG_CC_C_O
AC_HEADER_STDC
PKG_PROG_PKG_CONFIG([0.20]) 

# check for gtk
PKG_CHECK_MODULES([GTK], [gtk+-3.0 >= 2.90.1])
GTK_CFLAGS+=-DGTK_MULTIDEVICE_SAFE
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

# check for XInput
PKG_CHECK_MODULES([XINPUT], [xi >= 1.3])
AC_SUBST(XINPUT_CFLAGS)
AC_SUBST(XINPUT_LIBS)



# ***************************************************************************
# finally
# ***************************************************************************
AC_OUTPUT([
Makefile
src/Makefile
])
