/* 
 * Gromit -- a program for painting on the screen
 * Copyright (C) 2000 Simon Budig <Simon.Budig@unix-ag.org>
 *
 * MPX modifications Copyright (C) 2009 Christian Beier <dontmind@freeshell.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef GROMIT_MPX_H
#define GROMIT_MPX_H


#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>


#define GROMIT_MOUSE_EVENTS ( GDK_POINTER_MOTION_MASK | \
                              GDK_BUTTON_MOTION_MASK | \
                              GDK_BUTTON_PRESS_MASK | \
                              GDK_BUTTON_RELEASE_MASK )

#define GROMIT_WINDOW_EVENTS ( GROMIT_MOUSE_EVENTS | GDK_EXPOSURE_MASK )

/* Atoms used to control Gromit */
#define GA_CONTROL    gdk_atom_intern ("Gromit/control", FALSE)
#define GA_STATUS     gdk_atom_intern ("Gromit/status", FALSE)
#define GA_QUIT       gdk_atom_intern ("Gromit/quit", FALSE)
#define GA_ACTIVATE   gdk_atom_intern ("Gromit/activate", FALSE)
#define GA_DEACTIVATE gdk_atom_intern ("Gromit/deactivate", FALSE)
#define GA_TOGGLE     gdk_atom_intern ("Gromit/toggle", FALSE)
#define GA_VISIBILITY gdk_atom_intern ("Gromit/visibility", FALSE)
#define GA_CLEAR      gdk_atom_intern ("Gromit/clear", FALSE)
#define GA_RELOAD     gdk_atom_intern ("Gromit/reload", FALSE)
#define GA_UNDO       gdk_atom_intern ("Gromit/undo", FALSE)
#define GA_REDO       gdk_atom_intern ("Gromit/redo", FALSE)

#define GA_DATA       gdk_atom_intern ("Gromit/data", FALSE)
#define GA_TOGGLEDATA gdk_atom_intern ("Gromit/toggledata", FALSE)



typedef enum
{
  GROMIT_PEN,
  GROMIT_ERASER,
  GROMIT_RECOLOR
} GromitPaintType;

typedef struct
{
  GromitPaintType type;
  guint           width;
  gfloat          arrowsize;
  GdkColor        *paint_color;
  cairo_t         *paint_ctx;
  gdouble         pressure;
} GromitPaintContext;

typedef struct
{
  gint x;
  gint y;
  gint width;
} GromitStrokeCoordinate;

typedef struct
{
  gdouble      lastx;
  gdouble      lasty;
  guint32      motion_time;
  GList*       coordlist;
  GdkDevice*   device;
  guint        index;
  guint        state;
  GromitPaintContext *cur_context;
  gboolean     is_grabbed;
  gboolean     was_grabbed;
  GdkDevice*   lastslave;
} GromitDeviceData;


typedef struct
{
  GtkWidget   *win;
  GtkStatusIcon *trayicon;

  GdkCursor   *paint_cursor;
  GdkCursor   *erase_cursor;

  GdkDisplay  *display;
  GdkScreen   *screen;
  gboolean     xinerama;
  gboolean     composited;
  GdkWindow   *root;
  gchar       *hot_keyval;
  guint        hot_keycode;

  GdkColor    *white;
  GdkColor    *black;
  GdkColor    *red;

  GromitPaintContext *default_pen;
  GromitPaintContext *default_eraser;
 
  GHashTable  *tool_config;

  cairo_surface_t *backbuffer;
  cairo_surface_t *undobuffer;

  GHashTable  *devdatatable;
  gboolean     all_grabbed;

  guint        timeout_id;
  guint        modified;
  guint        delayed;
  guint        maxwidth;
  guint        width;
  guint        height;
  guint        client;
  guint        painted;
  gboolean     hidden;
  gboolean     debug;

  gchar       *clientdata;
} GromitData;


void toggle_visibility (GromitData *data);
void hide_window (GromitData *data);
void show_window (GromitData *data);

void parse_print_help (gpointer key, gpointer value, gpointer user_data);

void select_tool (GromitData *data, GdkDevice *master, GdkDevice *slave, guint state);

void snap_undo_state (GromitData *data);
void undo_drawing (GromitData *data);
void redo_drawing (GromitData *data);

void draw_line (GromitData *data, GdkDevice *dev, gint x1, gint y1, gint x2, gint y2);
void draw_arrow (GromitData *data, GdkDevice *dev, gint x1, gint y1, gint width, gfloat direction);
void clear_screen (GromitData *data);

gboolean coord_list_get_arrow_param (GromitData *data,
					    GdkDevice  *dev,
					    gint        search_radius,
					    gint       *ret_width,
					    gfloat     *ret_direction);
void coord_list_prepend (GromitData *data, GdkDevice* dev, gint x, gint y, gint width);
void coord_list_free (GromitData *data, GdkDevice* dev);


GromitPaintContext *paint_context_new (GromitData *data, GromitPaintType type,
				       GdkColor *fg_color, guint width, guint arrowsize);
void paint_context_free (GromitPaintContext *context);



#endif
