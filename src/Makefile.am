## Process this file with automake to produce Makefile.in

AM_CFLAGS = -Wall -Wextra -Wno-unused-parameter

AM_CPPFLAGS = \
	$(GTK3_CFLAGS) \
	$(APPINDICATOR_CFLAGS) \
	$(XINPUT2_CFLAGS) \
	$(X11_CFLAGS)

bin_PROGRAMS = gromit-mpx

gromit_mpx_SOURCES = \
	gromit-mpx.c \
	gromit-mpx.h \
	callbacks.c \
	callbacks.h \
	config.c \
	config.h \
	input.c \
	input.h \
	erase_cursor.xpm \
	paint_cursor.xpm 
