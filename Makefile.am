## Process this file with automake to produce Makefile.in

AM_CFLAGS = -Wall -Wextra -Wno-unused-parameter

AM_CPPFLAGS = \
	$(GTK3_CFLAGS) \
	$(XINPUT2_CFLAGS) \
	$(X11_CFLAGS)

bin_PROGRAMS = gromit-mpx

gromit_mpx_SOURCES = \
	src/gromit-mpx.c \
	src/gromit-mpx.h \
	src/callbacks.c \
	src/callbacks.h \
	src/config.c \
	src/config.h \
	src/input.c \
	src/input.h \
	src/erase_cursor.xpm \
	src/paint_cursor.xpm

doc_DATA = \
	README.md\
	AUTHORS\
	ChangeLog\
	NEWS

Applicationsdir = $(datadir)/applications/
Applications_DATA = data/gromit-mpx.desktop

pixmapdir = $(datadir)/pixmaps
pixmap_DATA = data/gromit-mpx.png \
              data/gromit-mpx.xpm

man_MANS = gromit-mpx.1

scalabledir = $(datadir)/icons/hicolor/scalable/apps
scalable_DATA = data/gromit-mpx.svg

configdir = $(sysconfdir)/gromit-mpx
config_DATA = data/gromit-mpx.cfg

EXTRA_DIST = $(doc_DATA) \
             $(Applications_DATA) \
             $(pixmap_DATA) \
             $(config_DATA) \
             COPYING
