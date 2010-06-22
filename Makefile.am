## Process this file with automake to produce Makefile.in

SUBDIRS = src 

gromit_mpxdocdir = $(datadir)/doc/gromit-mpx
gromit_mpxdoc_DATA = \
	README\
	COPYING\
	AUTHORS\
	ChangeLog\
	NEWS

Applicationsdir = $(datadir)/applications/
Applications_DATA = data/gromit-mpx.desktop

pixmapdir = $(datadir)/pixmaps
pixmap_DATA = data/gromit-mpx.xpm

configdir = $(sysconfdir)/gromit-mpx
config_DATA = data/gromit-mpx.cfg

EXTRA_DIST = $(gromit_mpxdoc_DATA) \
             $(Applications_DATA) \
             $(pixmap_DATA) \
             $(config_DATA)

