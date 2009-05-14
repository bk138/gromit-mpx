#
# very simple makefile for gromit
# 
# NOTE: you need these:
#         - GTK, GDK, Glib devel package version >= 2.16                                     
#         - libXi version 2 strongly suggested ;-)
#


.PHONY : clean
 
CFLAGS = $(shell pkg-config --cflags gtk+-2.0) -Wall -O2 -g 
CPPFLAGS=-I/opt/mpx/include -DXINPUT2
LDFLAGS= $(shell pkg-config --libs gtk+-2.0)



OBJS=gromit.o 
OUTFILE=gromit
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

gromit.o:  paint_cursor.xpm erase_cursor.xpm