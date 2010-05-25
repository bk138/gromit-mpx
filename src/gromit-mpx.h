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


#define GROMIT_MOUSE_EVENTS ( GDK_PROXIMITY_IN_MASK | \
                              GDK_PROXIMITY_OUT_MASK | \
                              GDK_BUTTON_MOTION_MASK | \
                              GDK_BUTTON_PRESS_MASK | \
                              GDK_BUTTON_RELEASE_MASK )

#define GROMIT_PAINT_AREA_EVENTS  ( GROMIT_MOUSE_EVENTS | GDK_EXPOSURE_MASK )

#define GROMIT_WINDOW_EVENTS ( GROMIT_PAINT_AREA_EVENTS )

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

#define GA_DATA       gdk_atom_intern ("Gromit/data", FALSE)
#define GA_TOGGLEDATA gdk_atom_intern ("Gromit/toggledata", FALSE)

/* fallback device name for config file */
#define DEFAULT_DEVICE_NAME "default"


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
  GdkColor       *fg_color;
  GdkGC          *paint_gc;
  GdkGC          *shape_gc;
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
  GList       *coordlist;
  GdkDevice   *device;
  guint        index;
  guint        state;
  GromitPaintContext *cur_context;
  gboolean     is_grabbed;
  gboolean     was_grabbed;
} GromitDeviceData;


typedef struct
{
  GtkWidget   *win;
  GtkWidget   *area;

  GdkCursor   *paint_cursor;
  GdkCursor   *erase_cursor;

  GdkPixmap   *pixmap;
  GdkDisplay  *display;
  GdkScreen   *screen;
  gboolean     xinerama;
  GdkWindow   *root;
  gchar       *hot_keyval;
  guint        hot_keycode;

  GdkColormap *cm;
  GdkColor    *white;
  GdkColor    *black;
  GdkColor    *red;

  GromitPaintContext *default_pen;
  GromitPaintContext *default_eraser;
 
  GHashTable  *tool_config;

  GdkBitmap   *shape;
  GdkGC       *shape_gc;
  GdkGCValues *shape_gcv;
  GdkColor    *transparent;
  GdkColor    *opaque;

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


void gromit_toggle_visibility (GromitData *data);

void gromit_release_grab (GromitData *data, GdkDevice *dev);
void gromit_acquire_grab (GromitData *data, GdkDevice *dev);
void gromit_toggle_grab (GromitData *data, int dev_id);
  
void parse_print_help (gpointer key, gpointer value, gpointer user_data);
void setup_input_devices (GromitData *data);

void mainapp_event_selection_get (GtkWidget          *widget,
				  GtkSelectionData   *selection_data,
				  guint               info,
				  guint               time,
				  gpointer            user_data);
void mainapp_event_selection_received (GtkWidget *widget,
				       GtkSelectionData *selection_data,
				       guint time,
				       gpointer user_data);

void gromit_select_tool (GromitData *data, GdkDevice *device, guint state);

void gromit_draw_line (GromitData *data, GdkDevice *dev, gint x1, gint y1, gint x2, gint y2);
void gromit_draw_arrow (GromitData *data, GdkDevice *dev, gint x1, gint y1, gint width, gfloat direction);
void gromit_clear_screen (GromitData *data);

gboolean gromit_coord_list_get_arrow_param (GromitData *data,
					    GdkDevice  *dev,
					    gint        search_radius,
					    gint       *ret_width,
					    gfloat     *ret_direction);

void gromit_coord_list_prepend (GromitData *data, GdkDevice* dev, gint x, gint y, gint width);
void gromit_coord_list_free (GromitData *data, GdkDevice* dev);



#endif
