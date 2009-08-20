#
# very simple makefile for gromit
# 
# NOTE: you need these:
#         - GTK, GDK, Glib devel package version >= 2.16                                     
#         - libXi version 2 strongly suggested ;-)
#


.PHONY : clean
 
CFLAGS = $(shell pkg-config --cflags gtk+-2.0) -Wall -O2 -g 
LDFLAGS= $(shell pkg-config --libs gtk+-2.0)


OBJS=gromit-mpx.o 
OUTFILE=gromit-mpx
PREFIX=/usr/local/


all: $(OUTFILE) 

$(OUTFILE):	$(OBJS) 	 
	$(CC) $(CPPFLAGS) $(CFLAGS) $(OBJS) -o $(OUTFILE) $(LDFLAGS)


# clean me up, scotty
clean:
	$(RM) $(OUTFILE) $(OBJS) *~


install: $(OUTFILE) 
	install -d $(PREFIX)/bin
	install -m 755  $(OUTFILE) $(PREFIX)/bin

gromit-mpx.o:  paint_cursor.xpm erase_cursor.xpm