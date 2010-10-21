## Process this file with automake to produce Makefile.in

AM_CFLAGS =\
	-Wall \
	$(GTK3_CFLAGS) \
	$(XINPUT2_CFLAGS)



bin_PROGRAMS = gromit-mpx

gromit_mpx_SOURCES = \
	gromit-mpx.c \
	gromit-mpx.h \
	callbacks.c \
	callbacks.h \
	config.c \
	config.h \
	init.c \
	init.h \
	erase_cursor.xpm \
	paint_cursor.xpm 

gromit_mpx_LDFLAGS = 

gromit_mpx_LDADD = \
	$(GTK3_LIBS) \
	$(XINPUT2_LIBS)



