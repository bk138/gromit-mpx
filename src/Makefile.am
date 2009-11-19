## Process this file with automake to produce Makefile.in

AM_CFLAGS =\
	-Wall \
	$(GTK_CFLAGS) \
	$(XINPUT_CFLAGS)



bin_PROGRAMS = gromit-mpx

gromit_mpx_SOURCES = \
	gromit-mpx.c \
	gromit-mpx.h \
	callbacks.c \
	callbacks.h \
	erase_cursor.xpm \
	paint_cursor.xpm

gromit_mpx_LDFLAGS = 

gromit_mpx_LDADD = \
	$(GTK_LIBS) \
	$(XINPUT_LIBS)



