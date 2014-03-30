## Process this file with automake to produce Makefile.in

SUBDIRS = src 

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
