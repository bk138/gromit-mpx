/* Gromit -- a program for painting on the screen
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
 */

#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkinput.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <X11/extensions/XInput.h>
#include <X11/Xcursor/Xcursor.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>

#include "paint_cursor.xpm"
#include "erase_cursor.xpm"


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
  XDevice     *xdevice; /* as long as gdk does not expose the X device, we store this here */
  guint        state;
  GromitPaintContext *cur_context;
  gboolean     is_grabbed;
  gboolean     was_grabbed;
} GromitDeviceData;


typedef struct
{
  GtkWidget   *win;
  GtkWidget   *area;
  GtkWidget   *panel;
  GtkWidget   *button;

  Cursor       paint_cursor;
  Cursor       erase_cursor;

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


/* I need a prototype...  */
void gromit_release_grab (GromitData *data, GdkDevice *dev);
void gromit_acquire_grab (GromitData *data, GdkDevice *dev);
void parse_print_help (gpointer key, gpointer value, gpointer user_data);
void setup_input_devices (GromitData *data);

GromitPaintContext *
gromit_paint_context_new (GromitData *data, GromitPaintType type,
                          GdkColor *fg_color, guint width, guint arrowsize)
{
  GromitPaintContext *context;
  GdkGCValues   shape_gcv;

  context = g_malloc (sizeof (GromitPaintContext));

  context->type = type;
  context->width = width;
  context->arrowsize = arrowsize;
  context->fg_color = fg_color;

  if (type == GROMIT_ERASER)
    {
      context->paint_gc = NULL;
    }
  else
    {
      /* GROMIT_PEN || GROMIT_RECOLOR */
      context->paint_gc = gdk_gc_new (data->pixmap);
      gdk_gc_set_foreground (context->paint_gc, fg_color);
      gdk_gc_set_line_attributes (context->paint_gc, width, GDK_LINE_SOLID,
                                  GDK_CAP_ROUND, GDK_JOIN_ROUND);
    }

  if (type == GROMIT_RECOLOR)
    {
      context->shape_gc = NULL;
    }
  else
    {
      /* GROMIT_PEN || GROMIT_ERASER */
      context->shape_gc = gdk_gc_new (data->shape);
      gdk_gc_get_values (context->shape_gc, &shape_gcv);

      if (type == GROMIT_ERASER)
         gdk_gc_set_foreground (context->shape_gc, &(shape_gcv.foreground));
      else
         /* GROMIT_PEN */
         gdk_gc_set_foreground (context->shape_gc, &(shape_gcv.background));
      gdk_gc_set_line_attributes (context->shape_gc, width, GDK_LINE_SOLID,
                                  GDK_CAP_ROUND, GDK_JOIN_ROUND);
    }

  return context;
}


void
gromit_paint_context_print (gchar *name, GromitPaintContext *context)
{
  g_printerr ("Tool name: \"%-20s\": ", name);
  switch (context->type)
  {
    case GROMIT_PEN:
      g_printerr ("Pen,     "); break;
    case GROMIT_ERASER:
      g_printerr ("Eraser,  "); break;
    case GROMIT_RECOLOR:
      g_printerr ("Recolor, "); break;
    default:
      g_printerr ("UNKNOWN, "); break;
  }

  g_printerr ("width: %3d, ", context->width);
  g_printerr ("arrowsize: %.2f, ", context->arrowsize);
  g_printerr ("color: #%02X%02X%02X\n", context->fg_color->red >> 8,
              context->fg_color->green >> 8, context->fg_color->blue >> 8);
}


void
gromit_paint_context_free (GromitPaintContext *context)
{
  g_object_unref (context->paint_gc);
  g_object_unref (context->shape_gc);
  g_free (context);
}


void
gromit_coord_list_prepend (GromitData *data, GdkDevice* dev, gint x, gint y, gint width)
{
  /* get the data for this device */
  GromitDeviceData *devdata = g_hash_table_lookup(data->devdatatable, dev);

  GromitStrokeCoordinate *point;

  point = g_malloc (sizeof (GromitStrokeCoordinate));
  point->x = x;
  point->y = y;
  point->width = width;

  devdata->coordlist = g_list_prepend (devdata->coordlist, point);
}


void
gromit_coord_list_free (GromitData *data, GdkDevice* dev)
{
  // get the data for this device
  GromitDeviceData *devdata = g_hash_table_lookup(data->devdatatable, dev);

  GList *ptr;
  ptr = devdata->coordlist;

  while (ptr)
    {
      g_free (ptr->data);
      ptr = ptr->next;
    }

  g_list_free (devdata->coordlist);

  devdata->coordlist = NULL;
}


gboolean
gromit_coord_list_get_arrow_param (GromitData *data,
                                   GdkDevice  *dev,
                                   gint        search_radius,
                                   gint       *ret_width,
                                   gfloat     *ret_direction)
{
  gint x0, y0, r2, dist;
  gboolean success = FALSE;
  GromitStrokeCoordinate  *cur_point, *valid_point;
  /* get the data for this device */
  GromitDeviceData *devdata = g_hash_table_lookup(data->devdatatable, dev);
  GList *ptr = devdata->coordlist;
  gfloat width;

  valid_point = NULL;

  if (ptr)
    {
      cur_point = ptr->data;
      x0 = cur_point->x;
      y0 = cur_point->y;
      r2 = search_radius * search_radius;
      dist = 0;

      while (ptr && dist < r2)
        {
          ptr = ptr->next;
          if (ptr)
            {
              cur_point = ptr->data;
              dist = (cur_point->x - x0) * (cur_point->x - x0) +
                     (cur_point->y - y0) * (cur_point->y - y0);
              width = cur_point->width * devdata->cur_context->arrowsize;
              if (width * 2 <= dist &&
                  (!valid_point || valid_point->width < cur_point->width))
                valid_point = cur_point;
            }
        }

      if (valid_point)
        {
          *ret_width = MAX (valid_point->width * devdata->cur_context->arrowsize,
                            2);
          *ret_direction = atan2 (y0 - valid_point->y, x0 - valid_point->x);
          success = TRUE;
        }
    }

  return success;
}


void
gromit_hide_window (GromitData *data)
{
  if (!data->hidden)
    {
      /* save grab state of each device */
      GHashTableIter it;
      gpointer value;
      GromitDeviceData* devdata; 
      g_hash_table_iter_init (&it, data->devdatatable);
      while (g_hash_table_iter_next (&it, NULL, &value)) 
        {
          devdata = value;
          devdata->was_grabbed = devdata->is_grabbed;
        }
      data->hidden = 1;
      gromit_release_grab (data, NULL); /* release all */
      gtk_widget_hide (data->win);
    }
}


void
gromit_show_window (GromitData *data)
{
  if (data->hidden)
    {
      gtk_widget_show (data->win);
      data->hidden = 0;

      /* restore grab state of each device */
      GHashTableIter it;
      gpointer value;
      GromitDeviceData* devdata; 
      g_hash_table_iter_init (&it, data->devdatatable);
      while (g_hash_table_iter_next (&it, NULL, &value)) 
        {
          devdata = value;
          if(devdata->was_grabbed)
            gromit_acquire_grab (data, devdata->device);
        }
    }
  gdk_window_raise (data->win->window);
}



void
gromit_toggle_visibility (GromitData *data)
{
  if (data->hidden)
    gromit_show_window (data);
  else
    gromit_hide_window (data);
}


void
gromit_release_grab (GromitData *data, GdkDevice *dev)
{
  if(!dev) /* this means release all grabs */
    {
      GHashTableIter it;
      gpointer value;
      GromitDeviceData* devdata; 
      g_hash_table_iter_init (&it, data->devdatatable);
      while (g_hash_table_iter_next (&it, NULL, &value)) 
        {
          devdata = value;
          if(!devdata->is_grabbed)
            continue;
	  {
	    gdk_error_trap_push ();
	    
	    XUngrabDevice(GDK_DISPLAY_XDISPLAY(data->display), devdata->xdevice, CurrentTime);
	    /* unset cursor */

	    XDefineDeviceCursor(GDK_DISPLAY_XDISPLAY(data->display), devdata->xdevice, GDK_WINDOW_XID(data->win->window), None);

	    gdk_flush ();
	    if (gdk_error_trap_pop ())
	      {
		/* this probably means the device table is outdated, e.g. this device doesn't exist anymore */
		g_printerr("Error ungrabbing Device %d while ungrabbing all, ignoring.\n", (int)devdata->xdevice->device_id);
		continue;
	      }
	  }
	  
          devdata->is_grabbed = 0;
        }
      
      data->all_grabbed = 0;

      if(data->debug)
        g_printerr ("DEBUG: Ungrabbed all Devices.\n");

      return;
    }


  /* get the data for this device */
  GromitDeviceData *devdata = g_hash_table_lookup(data->devdatatable, dev);

  if (devdata->is_grabbed)
    {
      {
	gdk_error_trap_push ();
	
	XUngrabDevice(GDK_DISPLAY_XDISPLAY(data->display), devdata->xdevice, CurrentTime);
	
	/* unset cursor */
	XDefineDeviceCursor(GDK_DISPLAY_XDISPLAY(data->display), devdata->xdevice, GDK_WINDOW_XID(data->win->window), None);
	
	gdk_flush ();
	if (gdk_error_trap_pop ())
	  {
              /* this probably means the device table is outdated, e.g. this device doesn't exist anymore */
              g_printerr("Error ungrabbing Device %d, rescanning device list.\n", (int)devdata->xdevice->device_id);
              setup_input_devices(data);
              return;
            }
        }

      devdata->is_grabbed = 0;

      if(data->debug)
        g_printerr ("DEBUG: Ungrabbed Device '%s'.\n", devdata->device->name);
    }

  if (!data->painted)
    gromit_hide_window (data);
}


void
gromit_acquire_grab (GromitData *data, GdkDevice *dev)
{
  gromit_show_window (data);

  /* classes for events we want to grab */
  int ignore;
  int num_classes = 3;
  XEventClass classes[num_classes];

  if(!dev) /* this means grab all */
    {
      GHashTableIter it;
      gpointer value;
      GromitDeviceData* devdata; 
      g_hash_table_iter_init (&it, data->devdatatable);
      while (g_hash_table_iter_next (&it, NULL, &value)) 
        {
          devdata = value;
          if(devdata->is_grabbed)
            continue;

	  gdk_error_trap_push();

              /* what do we want to grab? */
              DeviceButtonMotion (devdata->xdevice, ignore, classes[0]);
              DeviceButtonPress (devdata->xdevice, ignore, classes[1]);
              DeviceButtonRelease (devdata->xdevice, ignore, classes[2]);
              /*
              // these somehwo cause BadClass Errors :-(
              ProximityIn  (dev, ignore, classes[4]);
              ProximityOut  (dev, ignore, classes[5]);
              */
           
              XGrabDevice(GDK_DISPLAY_XDISPLAY(data->display), devdata->xdevice, 
                        GDK_WINDOW_XID(data->area->window), False, 
                        num_classes, classes, 
                        GrabModeAsync,  GrabModeAsync, CurrentTime);

	      if(devdata->cur_context && devdata->cur_context->type == GROMIT_ERASER)
		XDefineDeviceCursor(GDK_DISPLAY_XDISPLAY(data->display), devdata->xdevice, GDK_WINDOW_XID(data->win->window), data->erase_cursor); 
	      else
		XDefineDeviceCursor(GDK_DISPLAY_XDISPLAY(data->display), devdata->xdevice, GDK_WINDOW_XID(data->win->window), data->paint_cursor); 
	    	      
              gdk_flush ();
              if (gdk_error_trap_pop ())
                {
                  /* this probably means the device table is outdated, e.g. this device doesn't exist anymore */
                  g_printerr("Error grabbing Device %d while grabbing all, ignoring.\n", (int)devdata->xdevice->device_id);
                  continue;
                }

            

          devdata->is_grabbed = 1;
        }

      data->all_grabbed = 1;

      if(data->debug)
        g_printerr("DEBUG: Grabbed all Devices.\n");
      
      return;
    }


  /* get the data for this device */
  GromitDeviceData *devdata = g_hash_table_lookup(data->devdatatable, dev);

  if (!devdata->is_grabbed)
    {
          gdk_error_trap_push ();
	  
          /* what do we want to grab? */
          DeviceButtonMotion (devdata->xdevice, ignore, classes[0]);
          DeviceButtonPress (devdata->xdevice, ignore, classes[1]);
          DeviceButtonRelease (devdata->xdevice, ignore, classes[2]);
          /*
          // these somehow cause BadClass Errors :-(
          ProximityIn  (dev, ignore, classes[4]);
          ProximityOut  (dev, ignore, classes[5]);
          */
          XGrabDevice(GDK_DISPLAY_XDISPLAY(data->display), devdata->xdevice, 
                      GDK_WINDOW_XID(data->area->window), False, 
                      num_classes, classes, 
                      GrabModeAsync,  GrabModeAsync, CurrentTime);

	  if(devdata->cur_context && devdata->cur_context->type == GROMIT_ERASER)
	    XDefineDeviceCursor(GDK_DISPLAY_XDISPLAY(data->display), devdata->xdevice, GDK_WINDOW_XID(data->win->window), data->erase_cursor); 
	  else
	    XDefineDeviceCursor(GDK_DISPLAY_XDISPLAY(data->display), devdata->xdevice, GDK_WINDOW_XID(data->win->window), data->paint_cursor); 

          gdk_flush ();
          if (gdk_error_trap_pop ())
            {
              /* this probably means the device table is outdated, e.g. this device doesn't exist anymore */
              g_printerr("Error grabbing Device %d, rescanning device list.\n", (int)devdata->xdevice->device_id);
              setup_input_devices(data);
              return;
            }
        
      
      devdata->is_grabbed = 1;
      
      if(data->debug)
        g_printerr("DEBUG: Grabbed Device '%s'.\n", devdata->device->name);
    }

}

/* negative dev_id means  toggle all */
void
gromit_toggle_grab (GromitData *data, int dev_id)
{
  if(dev_id < 0)
    {
      if (data->all_grabbed)
	gromit_release_grab (data, NULL);
      else
	gromit_acquire_grab (data, NULL);
      return; 
    }
      
  /* find devdata with this device id */
  GHashTableIter it;
  gpointer value;
  GromitDeviceData* devdata = NULL; 
  g_hash_table_iter_init (&it, data->devdatatable);
  while (g_hash_table_iter_next (&it, NULL, &value)) 
    {
      devdata = value;
      if(devdata->xdevice->device_id == dev_id)
        break;
      else
        devdata = NULL;
    }
  
  if(devdata)
    {
      if (devdata->is_grabbed)
        gromit_release_grab (data, devdata->device);
      else
        gromit_acquire_grab (data, devdata->device);
    }
  else
    g_printerr("No Device with ID %d.\n", (int) dev_id);
}


void
gromit_clear_screen (GromitData *data)
{
  gdk_gc_set_foreground (data->shape_gc, data->transparent);
  gdk_draw_rectangle (data->shape, data->shape_gc, 1,
                      0, 0, data->width, data->height);
  gtk_widget_shape_combine_mask (data->win, data->shape, 0,0);

  data->painted = 0;

}


gint
reshape (gpointer user_data)
{
  GromitData *data = (GromitData *) user_data;

  if (data->modified)
    {
      if (gtk_events_pending () && data->delayed < 5)
        {
          data->delayed++ ;
        }
      else
        {
          gtk_widget_shape_combine_mask (data->win, data->shape, 0,0);
          data->modified = 0;
          data->delayed = 0;
        }
    }
  return 1;
}


void
gromit_select_tool (GromitData *data, GdkDevice *device, guint state)
{
  guint buttons = 0, modifier = 0, len = 0, default_len = 0;
  guint req_buttons = 0, req_modifier = 0;
  guint i, j, success = 0;
  GromitPaintContext *context = NULL;
  guchar *name;
  guchar *default_name;

  /* get the data for this device */
  GromitDeviceData *devdata = g_hash_table_lookup(data->devdatatable, device);
 
  if (device)
    {
      len = strlen (device->name);
      name = (guchar*) g_strndup (device->name, len + 3);
      default_len = strlen(DEFAULT_DEVICE_NAME);
      default_name = (guchar*) g_strndup (DEFAULT_DEVICE_NAME, default_len + 3);
      
      
      /* Extract Button/Modifiers from state (see GdkModifierType) */
      req_buttons = (state >> 8) & 31;

      req_modifier = (state >> 1) & 7;
      if (state & GDK_SHIFT_MASK) req_modifier |= 1;

      name [len] = 124;
      name [len+3] = 0;
      default_name [default_len] = 124;
      default_name [default_len+3] = 0;

      /*  0, 1, 3, 7, 15, 31 */
      context = NULL;
      i=-1;
      do
        {
          i++;
          buttons = req_buttons & ((1 << i)-1);
          j=-1;
          do
            {
              j++;
              modifier = req_modifier & ((1 << j)-1);
              name [len+1] = buttons + 64;
              name [len+2] = modifier + 48;
              default_name [default_len+1] = buttons + 64;
              default_name [default_len+2] = modifier + 48;
              context = g_hash_table_lookup (data->tool_config, name);
              if(context)
                {
                  if(data->debug)
                    g_printerr("DEBUG: Context %s set\n", name);
                  devdata->cur_context = context;
                  success = 1;
                }
              else /* try default_name */
                if((context = g_hash_table_lookup (data->tool_config, default_name)))
                  {
                    if(data->debug)
                      g_printerr("DEBUG: Context %s set\n", default_name);
                    devdata->cur_context = context;
                    success = 1;
                  }
                
            }
          while (j<=3 && req_modifier >= (1 << j));
        }
      while (i<=5 && req_buttons >= (1 << i));

      g_free (name);
      g_free (default_name);

      if (!success)
        {
          if (device->source == GDK_SOURCE_ERASER)
            devdata->cur_context = data->default_eraser;
          else
            devdata->cur_context = data->default_pen;
        }
    }
  else
    g_printerr ("ERROR: Attempt to select nonexistent device!\n");

  if(devdata->cur_context && devdata->cur_context->type == GROMIT_ERASER)
    XDefineDeviceCursor(GDK_DISPLAY_XDISPLAY(data->display), devdata->xdevice, 
			GDK_WINDOW_XID(data->win->window), data->erase_cursor); 
  
  else
      XDefineDeviceCursor(GDK_DISPLAY_XDISPLAY(data->display), devdata->xdevice, 
			  GDK_WINDOW_XID(data->win->window), data->paint_cursor); 
 

  devdata->state = state;
  devdata->device = device;
}


void
gromit_draw_line (GromitData *data, GdkDevice *dev, gint x1, gint y1,
                  gint x2, gint y2)
{
  GdkRectangle rect;
  GromitDeviceData *devdata = g_hash_table_lookup(data->devdatatable, dev);

  rect.x = MIN (x1,x2) - data->maxwidth / 2;
  rect.y = MIN (y1,y2) - data->maxwidth / 2;
  rect.width = ABS (x1-x2) + data->maxwidth;
  rect.height = ABS (y1-y2) + data->maxwidth;

  if (devdata->cur_context->paint_gc)
    gdk_gc_set_line_attributes (devdata->cur_context->paint_gc,
                                data->maxwidth, GDK_LINE_SOLID,
                                GDK_CAP_ROUND, GDK_JOIN_ROUND);
  if (devdata->cur_context->shape_gc)
    gdk_gc_set_line_attributes (devdata->cur_context->shape_gc,
                                data->maxwidth, GDK_LINE_SOLID,
                                GDK_CAP_ROUND, GDK_JOIN_ROUND);

  if (devdata->cur_context->paint_gc)
    gdk_draw_line (data->pixmap, devdata->cur_context->paint_gc,
                   x1, y1, x2, y2);

  if (devdata->cur_context->shape_gc)
    {
      gdk_draw_line (data->shape, devdata->cur_context->shape_gc,
                     x1, y1, x2, y2);
      data->modified = 1;
    }

  if (devdata->cur_context->paint_gc)
    gdk_window_invalidate_rect(data->area->window, &rect, 0); 

  data->painted = 1;
}


void
gromit_draw_arrow (GromitData *data, GdkDevice *dev, gint x1, gint y1,
                   gint width, gfloat direction)
{
  GdkRectangle rect;
  GdkPoint arrowhead [4];

  /* get the data for this device */
  GromitDeviceData *devdata = g_hash_table_lookup(data->devdatatable, dev);

  width = width / 2;

  /* I doubt that calculating the boundary box more exact is very useful */
  rect.x = x1 - 4 * width - 1;
  rect.y = y1 - 4 * width - 1;
  rect.width = 8 * width + 2;
  rect.height = 8 * width + 2;

  arrowhead [0].x = x1 + 4 * width * cos (direction);
  arrowhead [0].y = y1 + 4 * width * sin (direction);

  arrowhead [1].x = x1 - 3 * width * cos (direction)
                       + 3 * width * sin (direction);
  arrowhead [1].y = y1 - 3 * width * cos (direction)
                       - 3 * width * sin (direction);

  arrowhead [2].x = x1 - 2 * width * cos (direction);
  arrowhead [2].y = y1 - 2 * width * sin (direction);

  arrowhead [3].x = x1 - 3 * width * cos (direction)
                       - 3 * width * sin (direction);
  arrowhead [3].y = y1 + 3 * width * cos (direction)
                       - 3 * width * sin (direction);

  if (devdata->cur_context->paint_gc)
    gdk_gc_set_line_attributes (devdata->cur_context->paint_gc,
                                0, GDK_LINE_SOLID,
                                GDK_CAP_ROUND, GDK_JOIN_ROUND);

  if (devdata->cur_context->shape_gc)
    gdk_gc_set_line_attributes (devdata->cur_context->shape_gc,
                                0, GDK_LINE_SOLID,
                                GDK_CAP_ROUND, GDK_JOIN_ROUND);

  if (devdata->cur_context->paint_gc)
    {
      gdk_draw_polygon (data->pixmap, devdata->cur_context->paint_gc,
                        TRUE, arrowhead, 4);
      gdk_gc_set_foreground (devdata->cur_context->paint_gc, data->black);
      gdk_draw_polygon (data->pixmap, devdata->cur_context->paint_gc,
                        FALSE, arrowhead, 4);
      gdk_gc_set_foreground (devdata->cur_context->paint_gc,
                             devdata->cur_context->fg_color);
    }

  if (devdata->cur_context->shape_gc)
    {
      gdk_draw_polygon (data->shape, devdata->cur_context->shape_gc,
                        TRUE, arrowhead, 4);
      gdk_draw_polygon (data->shape, devdata->cur_context->shape_gc,
                        FALSE, arrowhead, 4);
      data->modified = 1;
    }

  if (devdata->cur_context->paint_gc)
    gdk_window_invalidate_rect(data->area->window, &rect, 0); 

  data->painted = 1;
}


/*
 * Event-Handlers to perform the drawing
 */

gboolean
proximity_in (GtkWidget *win, GdkEventProximity *ev, gpointer user_data)
{
  GromitData *data = (GromitData *) user_data;
  gint x, y;
  GdkModifierType state;

  gdk_window_get_pointer (data->win->window, &x, &y, &state);
  gromit_select_tool (data, ev->device, state);

  if(data->debug)
    g_printerr("DEBUG: prox in device  %s: \n", ev->device->name);
  return TRUE;
}


gboolean
proximity_out (GtkWidget *win, GdkEventProximity *ev, gpointer user_data)
{
  GromitData *data = (GromitData *) user_data;
  
  /* get the data for this device */
  GromitDeviceData *devdata = g_hash_table_lookup(data->devdatatable, ev->device);

  devdata->cur_context = data->default_pen;

  devdata->state = 0;
  devdata->device = NULL;

  if(data->debug)
    g_printerr("DEBUG: prox out device  %s: \n", ev->device->name);
  return FALSE;
}


gboolean
paint (GtkWidget *win, GdkEventButton *ev, gpointer user_data)
{
  GromitData *data = (GromitData *) user_data;
  gdouble pressure = 0.5;
  /* get the data for this device */
  GromitDeviceData *devdata = g_hash_table_lookup(data->devdatatable, ev->device);

  if(data->debug)
    g_printerr("DEBUG: Device '%s': Button %i Down at (x,y)=(%.2f : %.2f)\n", ev->device->name, ev->button, ev->x, ev->y);

  if (!devdata->is_grabbed)
    return FALSE;

 
  /* See GdkModifierType. Am I fixing a Gtk misbehaviour???  */
  ev->state |= 1 << (ev->button + 7);
  if (ev->state != devdata->state)
    gromit_select_tool (data, ev->device, ev->state);

  gdk_window_set_background (data->area->window,
                             devdata->cur_context->fg_color);

  devdata->lastx = ev->x;
  devdata->lasty = ev->y;
  devdata->motion_time = ev->time;

  if (ev->device->source == GDK_SOURCE_MOUSE)
    {
      data->maxwidth = devdata->cur_context->width;
    }
  else
    {
      gdk_event_get_axis ((GdkEvent *) ev, GDK_AXIS_PRESSURE, &pressure);
      data->maxwidth = (CLAMP (pressure * pressure,0,1) *
                        (double) devdata->cur_context->width);
    }
  if (ev->button <= 5)
    gromit_draw_line (data, ev->device, ev->x, ev->y, ev->x, ev->y);

  gromit_coord_list_prepend (data, ev->device, ev->x, ev->y, data->maxwidth);

  /* if (data->cur_context->shape_gc && !gtk_events_pending ())
     gtk_widget_shape_combine_mask (data->win, data->shape, 0,0); */

  return TRUE;
}


gboolean
paintto (GtkWidget *win,
         GdkEventMotion *ev,
         gpointer user_data)
{
  GromitData *data = (GromitData *) user_data;
  GdkTimeCoord **coords = NULL;
  int nevents;
  int i;
  gboolean ret;
  gdouble pressure = 0.5;
  // get the data for this device
  GromitDeviceData *devdata = g_hash_table_lookup(data->devdatatable, ev->device);


  if (!devdata->is_grabbed)
    return FALSE;
 

  if (ev->state != devdata->state)
    gromit_select_tool (data, ev->device, ev->state);

  ret = gdk_device_get_history (ev->device, ev->window,
                                devdata->motion_time, ev->time,
                                &coords, &nevents);

  // XXX gdk_device_get_history is corrupt atm
  nevents = 0;
  if (!data->xinerama && nevents > 0)
    {
      for (i=0; i < nevents; i++)
        {
          gdouble x, y;

          gdk_device_get_axis (ev->device, coords[i]->axes,
                               GDK_AXIS_PRESSURE, &pressure);
          if (pressure > 0)
            {
              if (ev->device->source == GDK_SOURCE_MOUSE)
                data->maxwidth = devdata->cur_context->width;
              else
                data->maxwidth = (CLAMP (pressure * pressure, 0, 1) *
                                  (double) devdata->cur_context->width);

              gdk_device_get_axis(ev->device, coords[i]->axes,
                                  GDK_AXIS_X, &x);
              gdk_device_get_axis(ev->device, coords[i]->axes,
                                  GDK_AXIS_Y, &y);

              gromit_draw_line (data, ev->device, devdata->lastx, devdata->lasty, x, y);

              gromit_coord_list_prepend (data, ev->device, x, y, data->maxwidth);
              devdata->lastx = x;
              devdata->lasty = y;
            }
        }

      devdata->motion_time = coords[nevents-1]->time;
      g_free (coords);
    }

  /* always paint to the current event coordinate. */
  gdk_event_get_axis ((GdkEvent *) ev, GDK_AXIS_PRESSURE, &pressure);

  if (pressure > 0)
    {
      if (ev->device->source == GDK_SOURCE_MOUSE)
         data->maxwidth = devdata->cur_context->width;
      else
         data->maxwidth = (CLAMP (pressure * pressure,0,1) *
                           (double)devdata->cur_context->width);
      gromit_draw_line (data, ev->device, devdata->lastx, devdata->lasty, ev->x, ev->y);

      gromit_coord_list_prepend (data, ev->device, ev->x, ev->y, data->maxwidth);
    }

  devdata->lastx = ev->x;
  devdata->lasty = ev->y;

  return TRUE;
}


gboolean
paintend (GtkWidget *win, GdkEventButton *ev, gpointer user_data)
{
  GromitData *data = (GromitData *) user_data;
  /* get the device data for this event */
  GromitDeviceData *devdata = g_hash_table_lookup(data->devdatatable, ev->device);

  gfloat direction = 0;
  gint width = 0;
  if(devdata->cur_context)
    width = devdata->cur_context->arrowsize * devdata->cur_context->width / 2;
   

  if ((ev->x != devdata->lastx) ||
      (ev->y != devdata->lasty))
     paintto (win, (GdkEventMotion *) ev, user_data);

  if (!devdata->is_grabbed)
    return FALSE;

  if (devdata->cur_context->arrowsize != 0 &&
      gromit_coord_list_get_arrow_param (data, ev->device, width * 3,
                                         &width, &direction))
    gromit_draw_arrow (data, ev->device, ev->x, ev->y, width, direction);

  gromit_coord_list_free (data, ev->device);

  return TRUE;
}


/*
 * Functions for handling various (GTK+)-Events
 */

void
quiet_print_handler (const gchar *string)
{
  return;
}


gboolean
event_configure (GtkWidget *widget,
                 GdkEventExpose *event,
                 gpointer user_data)
{
  GromitData *data = (GromitData *) user_data;

  data->pixmap = gdk_pixmap_new (data->area->window, data->width,
                                 data->height, -1);
  gdk_draw_rectangle (data->pixmap, data->area->style->black_gc,
                      1, 0, 0, data->width, data->height);
  gdk_window_set_transient_for (data->area->window, data->win->window);

  return TRUE;
}


gboolean
event_expose (GtkWidget *widget,
              GdkEventExpose *event,
              gpointer user_data)
{
  GromitData *data = (GromitData *) user_data;

  gdk_draw_drawable (data->area->window,
                     data->area->style->fg_gc[GTK_WIDGET_STATE (data->area)],
                     data->pixmap,
                     event->area.x, event->area.y,
                     event->area.x, event->area.y,
                     event->area.width, event->area.height);
  return TRUE;
}


/* Keyboard control */

gint
key_press_event (GtkWidget   *grab_widget,
                 GdkEventKey *event,
                 gpointer     func_data)
{
  GromitData *data = (GromitData *) func_data;


  if (event->type == GDK_KEY_PRESS &&
      event->hardware_keycode == data->hot_keycode)
    {
      if (event->state & GDK_SHIFT_MASK)
        gromit_clear_screen (data);
      else if (event->state & GDK_CONTROL_MASK)
        gromit_toggle_visibility (data);
      else if (event->state & GDK_MOD1_MASK)
        gtk_main_quit ();
      else
        gromit_toggle_grab (data, 0); 

      return TRUE;
    }
  return FALSE;
}


void
gromit_main_do_event (GdkEventAny *event,
                      GromitData  *data)
{

  if ((event->type == GDK_KEY_PRESS ||
       event->type == GDK_KEY_RELEASE) &&
      event->window == data->root &&
      ((GdkEventKey *) event)->hardware_keycode == data->hot_keycode)
    {
      /* redirect the event to our main window, so that GTK+ doesn't
       * throw it away (there is no GtkWidget for the root window...)
       */
      event->window = data->win->window;
      g_object_ref (data->win->window);
    }

  gtk_main_do_event ((GdkEvent *) event);
}

/* Remote control */
void
mainapp_event_selection_get (GtkWidget          *widget,
                             GtkSelectionData   *selection_data,
                             guint               info,
                             guint               time,
                             gpointer            user_data)
{
  GromitData *data = (GromitData *) user_data;
  
  gchar *uri = "OK";

  if (selection_data->target == GA_TOGGLE)
    {
      /* ask back client for device id */
      gtk_selection_convert (data->win, GA_DATA,
                             GA_TOGGLEDATA, time);
      gtk_main(); /* Wait for the response */
    }
  else if (selection_data->target == GA_VISIBILITY)
    gromit_toggle_visibility (data);
  else if (selection_data->target == GA_CLEAR)
    gromit_clear_screen (data);
  else if (selection_data->target == GA_RELOAD)
    setup_input_devices(data);
  else if (selection_data->target == GA_QUIT)
    gtk_main_quit ();
  else
    uri = "NOK";

   
  gtk_selection_data_set (selection_data,
                          selection_data->target,
                          8, (guchar*)uri, strlen (uri));
}


void
mainapp_event_selection_received (GtkWidget *widget,
                                  GtkSelectionData *selection_data,
                                  guint time,
                                  gpointer user_data)
{
  GromitData *data = (GromitData *) user_data;

  if(selection_data->length < 0)
    {
      if(data->debug)
        g_printerr("DEBUG: mainapp got no answer back from client.\n");
    }
  else
    {
      if (selection_data->target == GA_TOGGLEDATA )
        {
          if(data->debug)
            g_printerr("DEBUG: mainapp got toggle id '%s' back from client.\n", (gchar*)selection_data->data);
          gromit_toggle_grab (data, strtoull((gchar*)selection_data->data, NULL, 10));
        }
    }
 
  gtk_main_quit ();
}


void
clientapp_event_selection_get (GtkWidget          *widget,
                               GtkSelectionData   *selection_data,
                               guint               info,
                               guint               time,
                               gpointer            user_data)
{
  GromitData *data = (GromitData *) user_data;
  
  gchar *ans = "";

  if(data->debug)
    g_printerr("DEBUG: clientapp received request.\n");  

  if (selection_data->target == GA_TOGGLEDATA)
    {
      ans = data->clientdata;
    }
    
  gtk_selection_data_set (selection_data,
                          selection_data->target,
                          8, (guchar*)ans, strlen (ans));
}


void
clientapp_event_selection_received (GtkWidget *widget,
                          GtkSelectionData *selection_data,
                          guint time,
                          gpointer user_data)
{
  GromitData *data = (GromitData *) user_data;

  /* If someone has a selection for us, Gromit is already running. */

  if (selection_data->type == GDK_NONE)
    data->client = 0;
  else
    data->client = 1;

  gtk_main_quit ();
}


/*
 * Functions for parsing the Configuration-file
 */

gchar *
parse_name (GScanner *scanner)
{
  GTokenType token;

  guint buttons = 0;
  guint modifier = 0;
  guint len = 0;
  gchar *name;

  token = g_scanner_cur_token(scanner);

  if (token != G_TOKEN_STRING)
    {
      g_scanner_unexp_token (scanner, G_TOKEN_STRING, NULL,
                             NULL, NULL, "aborting", TRUE);
      exit (1);
    }

  len = strlen (scanner->value.v_string);
  name = g_strndup (scanner->value.v_string, len + 3);

  token = g_scanner_get_next_token (scanner);

  /*
   * Are there any options to limit the scope of the definition?
   */

  if (token == G_TOKEN_LEFT_BRACE)
    {
      g_scanner_set_scope (scanner, 1);
      scanner->config->int_2_float = 0;
      modifier = buttons = 0;
      while ((token = g_scanner_get_next_token (scanner))
             != G_TOKEN_RIGHT_BRACE)
        {
          if (token == G_TOKEN_SYMBOL)
            {
              if ((guint) scanner->value.v_symbol < 11)
                 buttons |= 1 << ((guint) scanner->value.v_symbol - 1);
              else
                 modifier |= 1 << ((guint) scanner->value.v_symbol - 11);
            }
          else if (token == G_TOKEN_INT)
            {
              if (scanner->value.v_int <= 5 && scanner->value.v_int > 0)
                buttons |= 1 << (scanner->value.v_int - 1);
              else
                g_printerr ("Only Buttons 1-5 are supported!\n");
            }
          else
            {
              g_printerr ("skipped token\n");
            }
        }
      g_scanner_set_scope (scanner, 0);
      scanner->config->int_2_float = 1;
      token = g_scanner_get_next_token (scanner);
    }

  name [len] = 124;
  name [len+1] = buttons + 64;
  name [len+2] = modifier + 48;
  name [len+3] = 0;

  return name;
}

void
parse_config (GromitData *data)
{
  GromitPaintContext *context=NULL;
  GromitPaintContext *context_template=NULL;
  GScanner *scanner;
  GTokenType token;
  gchar *filename;
  int file;

  gchar *name, *copy;

  GromitPaintType type;
  GdkColor *fg_color=NULL;
  guint width, arrowsize;

  filename = g_strjoin (G_DIR_SEPARATOR_S,
                        g_get_home_dir(), ".gromitrc", NULL);
  file = open (filename, O_RDONLY);

  if (file < 0)
    {
      /* try global config file */
      g_free (filename);
      filename = g_strdup ("/etc/gromit/gromitrc");
      file = open (filename, O_RDONLY);

      if (file < 0)
        {
          g_printerr ("Could not open %s: %s\n", filename, g_strerror (errno));
          g_free (filename);
          return;
        }
    }

  scanner = g_scanner_new (NULL);
  scanner->input_name = filename;
  scanner->config->case_sensitive = 0;
  scanner->config->scan_octal = 0;
  scanner->config->identifier_2_string = 0;
  scanner->config->char_2_token = 1;
  scanner->config->numbers_2_int = 1;
  scanner->config->int_2_float = 1;

  g_scanner_scope_add_symbol (scanner, 0, "PEN",    (gpointer) GROMIT_PEN);
  g_scanner_scope_add_symbol (scanner, 0, "ERASER", (gpointer) GROMIT_ERASER);
  g_scanner_scope_add_symbol (scanner, 0, "RECOLOR",(gpointer) GROMIT_RECOLOR);

  g_scanner_scope_add_symbol (scanner, 1, "BUTTON1", (gpointer) 1);
  g_scanner_scope_add_symbol (scanner, 1, "BUTTON2", (gpointer) 2);
  g_scanner_scope_add_symbol (scanner, 1, "BUTTON3", (gpointer) 3);
  g_scanner_scope_add_symbol (scanner, 1, "BUTTON4", (gpointer) 4);
  g_scanner_scope_add_symbol (scanner, 1, "BUTTON5", (gpointer) 5);
  g_scanner_scope_add_symbol (scanner, 1, "SHIFT",   (gpointer) 11);
  g_scanner_scope_add_symbol (scanner, 1, "CONTROL", (gpointer) 12);
  g_scanner_scope_add_symbol (scanner, 1, "META",    (gpointer) 13);
  g_scanner_scope_add_symbol (scanner, 1, "ALT",     (gpointer) 13);

  g_scanner_scope_add_symbol (scanner, 2, "size",      (gpointer) 1);
  g_scanner_scope_add_symbol (scanner, 2, "color",     (gpointer) 2);
  g_scanner_scope_add_symbol (scanner, 2, "arrowsize", (gpointer) 3);

  g_scanner_set_scope (scanner, 0);
  scanner->config->scope_0_fallback = 0;

  g_scanner_input_file (scanner, file);

  token = g_scanner_get_next_token (scanner);
  while (token != G_TOKEN_EOF)
    {

      /*
       * New tool definition
       */

      if (token == G_TOKEN_STRING)
        {
          name = parse_name (scanner);
          token = g_scanner_cur_token(scanner);

          if (token != G_TOKEN_EQUAL_SIGN)
            {
              g_scanner_unexp_token (scanner, G_TOKEN_EQUAL_SIGN, NULL,
                                     NULL, NULL, "aborting", TRUE);
              exit (1);
            }

          token = g_scanner_get_next_token (scanner);

          /* defaults */

          type = GROMIT_PEN;
          width = 7;
          arrowsize = 0;
          fg_color = data->red;

          if (token == G_TOKEN_SYMBOL)
            {
              type = (GromitPaintType) scanner->value.v_symbol;
              token = g_scanner_get_next_token (scanner);
            }
          else if (token == G_TOKEN_STRING)
            {
              copy = parse_name (scanner);
              token = g_scanner_cur_token(scanner);
              context_template = g_hash_table_lookup (data->tool_config, copy);
              if (context_template)
                {
                  type = context_template->type;
                  width = context_template->width;
                  arrowsize = context_template->arrowsize;
                  fg_color = context_template->fg_color;
                }
              else
                {
                  g_printerr ("WARNING: Unable to copy \"%s\": "
                              "not yet defined!\n", copy);
                }
            }
          else
            {
              g_printerr ("Expected Tool-definition "
                          "or name of template tool\n");
              exit (1);
            }

          /* Are there any tool-options?
           */

          if (token == G_TOKEN_LEFT_PAREN)
            {
              GdkColor *color = NULL;
              g_scanner_set_scope (scanner, 2);
              scanner->config->int_2_float = 1;
              token = g_scanner_get_next_token (scanner);
              while (token != G_TOKEN_RIGHT_PAREN)
                {
                  if (token == G_TOKEN_SYMBOL)
                    {
                      if ((guint) scanner->value.v_symbol == 1)
                        {
                          token = g_scanner_get_next_token (scanner);
                          if (token != G_TOKEN_EQUAL_SIGN)
                            {
                              g_printerr ("Missing \"=\"... aborting\n");
                              exit (1);
                            }
                          token = g_scanner_get_next_token (scanner);
                          if (token != G_TOKEN_FLOAT)
                            {
                              g_printerr ("Missing Size (float)... aborting\n");
                              exit (1);
                            }
                          width = (guint) (scanner->value.v_float + 0.5);
                        }
                      else if ((guint) scanner->value.v_symbol == 2)
                        {
                          token = g_scanner_get_next_token (scanner);
                          if (token != G_TOKEN_EQUAL_SIGN)
                            {
                              g_printerr ("Missing \"=\"... aborting\n");
                              exit (1);
                            }
                          token = g_scanner_get_next_token (scanner);
                          if (token != G_TOKEN_STRING)
                            {
                              g_printerr ("Missing Color (string)... "
                                          "aborting\n");
                              exit (1);
                            }
                          color = g_malloc (sizeof (GdkColor));
                          if (gdk_color_parse (scanner->value.v_string,
                                               color))
                            {
                              if (gdk_colormap_alloc_color (data->cm,
                                                            color, 0, 1))
                                {
                                  fg_color = color;
                                }
                              else
                                {
                                  g_printerr ("Unable to allocate color. "
                                              "Keeping default!\n");
                                  g_free (color);
                                }
                            }
                          else
                            {
                              g_printerr ("Unable to parse color. "
                                          "Keeping default.\n");
                              g_free (color);
                            }
                          color = NULL;
                        }
                      else if ((guint) scanner->value.v_symbol == 3)
                        {
                          token = g_scanner_get_next_token (scanner);
                          if (token != G_TOKEN_EQUAL_SIGN)
                            {
                              g_printerr ("Missing \"=\"... aborting\n");
                              exit (1);
                            }
                          token = g_scanner_get_next_token (scanner);
                          if (token != G_TOKEN_FLOAT)
                            {
                              g_printerr ("Missing Arrowsize (float)... "
                                          "aborting\n");
                              exit (1);
                            }
                          arrowsize = scanner->value.v_float;
                        }
                      else
                        {
                          g_printerr ("Unknown tool type?????\n");
                        }
                    }
                  else
                    {
                      g_printerr ("skipped token!!!\n");
                    }
                  token = g_scanner_get_next_token (scanner);
                }
              g_scanner_set_scope (scanner, 0);
              token = g_scanner_get_next_token (scanner);
            }

          /*
           * Finally we expect a semicolon
           */

          if (token != ';')
            {
              g_printerr ("Expected \";\"\n");
              exit (1);
            }

          context = gromit_paint_context_new (data, type, fg_color, width, arrowsize);
          g_hash_table_insert (data->tool_config, name, context);
        }
      else
        {
          g_printerr ("Expected name of Tool to define\n");
          exit(1);
        }

      token = g_scanner_get_next_token (scanner);
    }
  g_scanner_destroy (scanner);
  close (file);
  g_free (filename);
}


/*
 * Functions for setting up (parts of) the application
 */



void
setup_input_devices (GromitData *data)
{
  GList *tmp_list;
  XDeviceInfo	*devices;
  int		num_devices, i;
  devices = XListInputDevices(GDK_DISPLAY_XDISPLAY(data->display), &num_devices); 

  /* 
     this is for my patched libgdk which reads devices anew each call of gdk_display_list_devices(),
     but shouldn't harm with vanilla gdk 
  */
  gromit_release_grab (data, NULL); /* ungrab all */
  g_hash_table_remove_all(data->devdatatable);

  for (tmp_list = gdk_display_list_devices (data->display);
       tmp_list;
       tmp_list = tmp_list->next)
    {
      GdkDevice *device = (GdkDevice *) tmp_list->data;

    
      /* Guess "Eraser"-Type devices */
      if (strstr (device->name, "raser") ||
          strstr (device->name, "RASER"))
         gdk_device_set_source (device, GDK_SOURCE_ERASER);

      /* only enable devices with 2 ore more axes */
     
      if (device->num_axes >= 2)
        {
          gdk_device_set_mode (device, GDK_MODE_SCREEN);

          GromitDeviceData *devdata;
          devdata  = g_malloc0(sizeof (GromitDeviceData));
          devdata->device = device;

          /* 
             we need to determine the XDevice for the GdkDevice
             the name->name mapping ain't nice, but what can i do...
          */
          for(i = 0; i < num_devices; ++i) /* seems the InputDevices List is already chronologically reversed */
            if(strcmp(devices[i].name, device->name) == 0)
              {
		/* check if we have a device with this id already in case of name duplication */
		gboolean dup = 0;
		GHashTableIter it;
		GromitDeviceData *dd;
		gpointer value;
		g_hash_table_iter_init (&it, data->devdatatable);
		while (g_hash_table_iter_next (&it, NULL, &value)) 
		  {
		    dd = value;
		    if(dd->xdevice->device_id == devices[i].id)
		      dup = 1;
		  }
		
		if(dup)
		  continue; /* further search in devices[] */

		devdata->xdevice = XOpenDevice(GDK_DISPLAY_XDISPLAY(data->display), devices[i].id);
                break;
              }

	  if(!devdata->xdevice) /* very bad, gdk_display_list_devices() has a device that doesn't exist anymore */
	    {
	      g_free(devdata);
	      return;
	    }

         
	  g_hash_table_insert(data->devdatatable, device, devdata);
          g_printerr ("Enabled Device %d: \"%s\" (Type: %d)\n", (int)devdata->xdevice->device_id, device->name, device->source);
        }
    }

  XFreeDeviceList(devices);
  g_printerr ("Now %d enabled devices.\n", g_hash_table_size(data->devdatatable));

}


/* 
   converts a rgb pixbuf to a XCursorImage 
*/
XcursorImage* rgb_pixbuf_2_cursor_image (GdkPixbuf *pixbuf,
					  gint       xhot,
					  gint       yhot)
{
  guint width, height, rowstride, n_channels, bpp;
  guchar *pixels, *src;
  XcursorImage *xcimage;
  XcursorPixel *dest;

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);
  n_channels = gdk_pixbuf_get_n_channels (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  pixels = gdk_pixbuf_get_pixels (pixbuf);
  bpp = gdk_pixbuf_get_bits_per_sample(pixbuf); 

  xcimage = XcursorImageCreate (width, height);

  xcimage->xhot = xhot;
  xcimage->yhot = yhot;

  dest = xcimage->pixels; /* ARGB */

  if(bpp == 8)
    {
      gint i, row;
      for (row = 0; row < height; row++)
        {
          src = pixels + row * rowstride;
          for (i = 0; i < width; i++)
            {
	      if(n_channels == 3)
		*dest = (0xff << 24) | (src[0] << 16) | (src[1] << 8) | src[2];
	      else /* pixbuf is in RGBA */
		*dest = (src[3] << 24) | (src[0] << 16) | (src[1] << 8) | src[2];
	   
	      src += n_channels;
	      ++dest;
	    }
	}
    }
  else
    return NULL;

  return xcimage;
}




void
setup_client_app (GromitData *data)
{
  data->display = gdk_display_get_default ();
  data->screen = gdk_display_get_default_screen (data->display);
  data->xinerama = gdk_screen_get_n_monitors (data->screen) > 1;
  data->root = gdk_screen_get_root_window (data->screen);
  data->width = gdk_screen_get_width (data->screen);
  data->height = gdk_screen_get_height (data->screen);


  data->win = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_widget_set_usize (GTK_WIDGET (data->win), data->width, data->height);
  gtk_widget_set_uposition (GTK_WIDGET (data->win), 0, 0);
  
  gtk_widget_set_events (data->win, GROMIT_WINDOW_EVENTS);

  g_signal_connect (data->win, "delete-event", gtk_main_quit, NULL);
  g_signal_connect (data->win, "destroy", gtk_main_quit, NULL);
  /* the selectione event handlers will be overwritten if we become a mainapp */
  g_signal_connect (data->win, "selection_received", 
		    G_CALLBACK (clientapp_event_selection_received), data);
  g_signal_connect (data->win, "selection_get",
		    G_CALLBACK (clientapp_event_selection_get), data);
  
  gtk_widget_realize (data->win); 

  gtk_selection_owner_set (data->win, GA_DATA, GDK_CURRENT_TIME);
  gtk_selection_add_target (data->win, GA_DATA, GA_TOGGLEDATA, 1007);
}


void
setup_main_app (GromitData *data, gboolean activate)
{
  GdkPixbuf *cursor_src;
  XcursorImage *cursor_image = NULL;
  gboolean   have_key = FALSE;

  data->devdatatable = g_hash_table_new(NULL, NULL);
  setup_input_devices (data);


  /* COLORMAP */
  data->cm = gdk_screen_get_default_colormap (data->screen);
  data->white = g_malloc (sizeof (GdkColor));
  data->black = g_malloc (sizeof (GdkColor));
  data->red   = g_malloc (sizeof (GdkColor));
  gdk_color_parse ("#FFFFFF", data->white);
  gdk_colormap_alloc_color (data->cm, data->white, FALSE, TRUE);
  gdk_color_parse ("#000000", data->black);
  gdk_colormap_alloc_color (data->cm, data->black, FALSE, TRUE);
  gdk_color_parse ("#FF0000", data->red);
  gdk_colormap_alloc_color (data->cm, data->red,  FALSE, TRUE);

  /* CURSORS */
  cursor_src = gdk_pixbuf_new_from_xpm_data (paint_cursor_xpm);
  cursor_image = rgb_pixbuf_2_cursor_image(cursor_src, paint_cursor_x_hot,  paint_cursor_y_hot);
  data->paint_cursor = XcursorImageLoadCursor(GDK_DISPLAY_XDISPLAY (data->display), cursor_image);
  g_object_unref (cursor_src);
  XcursorImageDestroy(cursor_image);

  cursor_src = gdk_pixbuf_new_from_xpm_data(erase_cursor_xpm);
  cursor_image = rgb_pixbuf_2_cursor_image(cursor_src, erase_cursor_x_hot,  erase_cursor_y_hot);
  data->erase_cursor = XcursorImageLoadCursor(GDK_DISPLAY_XDISPLAY (data->display), cursor_image);
  g_object_unref (cursor_src);
  XcursorImageDestroy(cursor_image);     

  
  /* SHAPE PIXMAP */
  data->shape = gdk_pixmap_new (NULL, data->width, data->height, 1);
  data->shape_gc = gdk_gc_new (data->shape);
  data->shape_gcv = g_malloc (sizeof (GdkGCValues));
  gdk_gc_get_values (data->shape_gc, data->shape_gcv);
  data->transparent = gdk_color_copy (&(data->shape_gcv->foreground));
  data->opaque = gdk_color_copy (&(data->shape_gcv->background));
  gdk_gc_set_foreground (data->shape_gc, data->transparent);
  gdk_draw_rectangle (data->shape, data->shape_gc,
                      1, 0, 0, data->width, data->height);

  /* DRAWING AREA */
  data->area = gtk_drawing_area_new ();
  gtk_drawing_area_size (GTK_DRAWING_AREA (data->area),
                         data->width, data->height);

  /* EVENTS */
  gtk_widget_set_events (data->area, GROMIT_PAINT_AREA_EVENTS);
  g_signal_connect (data->area, "expose_event",
		    G_CALLBACK (event_expose), data);
  g_signal_connect (data->area,"configure_event",
		    G_CALLBACK (event_configure), data);
  g_signal_connect (data->win, "motion_notify_event",
		    G_CALLBACK (paintto), data);
  g_signal_connect (data->win, "button_press_event", 
		    G_CALLBACK(paint), data);
  g_signal_connect (data->win, "button_release_event",
		    G_CALLBACK (paintend), data);
  g_signal_connect (data->win, "proximity_in_event",
		    G_CALLBACK (proximity_in), data);
  g_signal_connect (data->win, "proximity_out_event",
		    G_CALLBACK (proximity_out), data);
  /* disconnect previously defined selection handlers */
  g_signal_handlers_disconnect_by_func (data->win, 
					G_CALLBACK (clientapp_event_selection_get),
					data);
  g_signal_handlers_disconnect_by_func (data->win, 
					G_CALLBACK (clientapp_event_selection_received),
					data);
  /* and re-connect them to mainapp functions */
  g_signal_connect (data->win, "selection_get",
		    G_CALLBACK (mainapp_event_selection_get), data);
  g_signal_connect (data->win, "selection_received",
		    G_CALLBACK (mainapp_event_selection_received), data);

  gtk_widget_set_extension_events (data->area, GDK_EXTENSION_EVENTS_ALL);
 


  gtk_container_add (GTK_CONTAINER (data->win), data->area);

  gtk_widget_shape_combine_mask (data->win, data->shape, 0,0);

  gtk_widget_show_all (data->area);

  data->painted = 0;
  gromit_hide_window (data);

  data->timeout_id = gtk_timeout_add (20, reshape, data);
 
  data->modified = 0;

  data->default_pen = gromit_paint_context_new (data, GROMIT_PEN,
                                                data->red, 7, 0);
  data->default_eraser = gromit_paint_context_new (data, GROMIT_ERASER,
                                                   data->red, 75, 0);

  
  /*
   * Parse Config file
   */

  data->tool_config = g_hash_table_new (g_str_hash, g_str_equal);
  parse_config (data);
  if (data->debug)
    g_hash_table_foreach (data->tool_config, parse_print_help, NULL);
  
  
  /* reset settings from client setup */
  gtk_selection_remove_all (data->win);
  gtk_selection_owner_set (data->win, GA_CONTROL, GDK_CURRENT_TIME);

  gtk_selection_add_target (data->win, GA_CONTROL, GA_STATUS, 0);
  gtk_selection_add_target (data->win, GA_CONTROL, GA_QUIT, 1);
  gtk_selection_add_target (data->win, GA_CONTROL, GA_ACTIVATE, 2);
  gtk_selection_add_target (data->win, GA_CONTROL, GA_DEACTIVATE, 3);
  gtk_selection_add_target (data->win, GA_CONTROL, GA_TOGGLE, 4);
  gtk_selection_add_target (data->win, GA_CONTROL, GA_VISIBILITY, 5);
  gtk_selection_add_target (data->win, GA_CONTROL, GA_CLEAR, 6);
  gtk_selection_add_target (data->win, GA_CONTROL, GA_RELOAD, 7);



  /* Grab the GROMIT_HOTKEY */

  if (data->hot_keyval)
    {
      GdkKeymap    *keymap;
      GdkKeymapKey *keys;
      gint          n_keys;
      guint         keyval;

      if (strlen (data->hot_keyval) > 0 &&
          strcasecmp (data->hot_keyval, "none") != 0)
        {
          keymap = gdk_keymap_get_for_display (data->display);
          keyval = gdk_keyval_from_name (data->hot_keyval);

          if (!keyval || !gdk_keymap_get_entries_for_keyval (keymap, keyval,
                                                             &keys, &n_keys))
            {
              g_printerr ("cannot find the key \"%s\"\n", data->hot_keyval);
              exit (1);
            }

          have_key = TRUE;
          data->hot_keycode = keys[0].keycode;
          g_free (keys);
        }
    }

  if (have_key)
    {
      if (data->hot_keycode)
        {
          gdk_error_trap_push ();

          XGrabKey (GDK_DISPLAY_XDISPLAY (data->display),
                    data->hot_keycode,
                    AnyModifier,
                    GDK_WINDOW_XWINDOW (data->root),
                    TRUE,
                    GrabModeAsync,
                    GrabModeAsync);

          gdk_flush ();

          if (gdk_error_trap_pop ())
            {
              g_printerr ("could not grab Hotkey. Aborting...\n");
              exit (1);
            }
        }
      else
        {
          g_printerr ("cannot find the key #%d\n", data->hot_keycode);
          exit (1);
        }
    }

  gdk_event_handler_set ((GdkEventFunc) gromit_main_do_event, data, NULL);
  gtk_key_snooper_install (key_press_event, data);

  if (activate)
    gromit_acquire_grab (data, NULL); /* grab all */
}

void
parse_print_help (gpointer key, gpointer value, gpointer user_data)
{
  gromit_paint_context_print ((gchar *) key, (GromitPaintContext *) value);
}


int
app_parse_args (int argc, char **argv, GromitData *data)
{
   gint      i;
   gchar    *arg;
   gboolean  wrong_arg = FALSE;
   gboolean  activate = FALSE;

   data->hot_keyval = "Pause";
   data->hot_keycode = 0;

   for (i=1; i < argc ; i++)
     {
       arg = argv[i];
       if (strcmp (arg, "-a") == 0 ||
           strcmp (arg, "--active") == 0)
         {
           activate = TRUE;
         }
       else if (strcmp (arg, "-d") == 0 ||
                strcmp (arg, "--debug") == 0)
         {
           data->debug = 1;
         }
       else if (strcmp (arg, "-k") == 0 ||
                strcmp (arg, "--key") == 0)
         {
           if (i+1 < argc)
             {
               data->hot_keyval = argv[i+1];
               data->hot_keycode = 0;
               i++;
             }
           else
             {
               g_printerr ("-k requires an Key-Name as argument\n");
               wrong_arg = TRUE;
             }
         }
       else if (strcmp (arg, "-K") == 0 ||
                strcmp (arg, "--keycode") == 0)
         {
           if (i+1 < argc && atoi (argv[i+1]) > 0)
             {
               data->hot_keyval = NULL;
               data->hot_keycode = atoi (argv[i+1]);
               i++;
             }
           else
             {
               g_printerr ("-K requires an keycode > 0 as argument\n");
               wrong_arg = TRUE;
             }
         }
       else
         {
           g_printerr ("Unknown Option for Gromit startup: \"%s\"\n", arg);
           wrong_arg = TRUE;
         }

       if (wrong_arg)
         {
           g_printerr ("Please see the Gromit manpage for the correct usage\n");
           exit (1);
         }
     }

   return activate;
}


/*
 * Main programs
 */

int
main_client (int argc, char **argv, GromitData *data)
{
   GdkAtom   action = GDK_NONE;
   gint      i;
   gchar    *arg;
   gboolean  wrong_arg = FALSE;

   for (i=1; i < argc ; i++)
     {
       arg = argv[i];
       action = GDK_NONE;
       if (strcmp (arg, "-d") == 0 ||
           strcmp (arg, "--debug") == 0)
         {
           data->debug = 1;
         }
       else if (strcmp (arg, "-t") == 0 ||
           strcmp (arg, "--toggle") == 0)
         {
           action = GA_TOGGLE;
           if (i+1 < argc && argv[i+1][0] != '-') /* there is an id supplied */
             {
               data->clientdata  = argv[i+1];
               ++i;
             }
           else
             data->clientdata = "-1"; /* default to grab all */
         }
       else if (strcmp (arg, "-v") == 0 ||
                strcmp (arg, "--visibility") == 0)
         {
           action = GA_VISIBILITY;
         }
       else if (strcmp (arg, "-q") == 0 ||
                strcmp (arg, "--quit") == 0)
         {
           action = GA_QUIT;
         }
       else if (strcmp (arg, "-c") == 0 ||
                strcmp (arg, "--clear") == 0)
         {
           action = GA_CLEAR;
         }
       else if (strcmp (arg, "-r") == 0 ||
                strcmp (arg, "--reload") == 0)
         {
           action = GA_RELOAD;
         }
       else
         {
           g_printerr ("Unknown Option to control a running Gromit process: \"%s\"\n", arg);
           wrong_arg = TRUE;
         }

       if (!wrong_arg && action != GDK_NONE)
         {
           gtk_selection_convert (data->win, GA_CONTROL,
                                  action, GDK_CURRENT_TIME);
           gtk_main ();  /* Wait for the response */
         }
       else if(wrong_arg)
         {
           g_printerr ("Please see the Gromit manpage for the correct usage\n");
           return 1;
         }
     }


   return 0;
}

int
main (int argc, char **argv)
{
  GromitData *data;

  gtk_init (&argc, &argv);
  data = g_malloc0(sizeof (GromitData));


  /* g_set_printerr_handler (quiet_print_handler); */

  setup_client_app (data);

  /* Try to get a status message. If there is a response gromit
   * is already acive.
   */

  gtk_selection_convert (data->win, GA_CONTROL, GA_STATUS,
                         GDK_CURRENT_TIME);
  gtk_main ();  /* Wait for the response */

  if (data->client)
    return main_client (argc, argv, data);

  /* Main application */
  setup_main_app (data, app_parse_args (argc, argv, data));
  gtk_main ();
  gromit_release_grab(data, NULL); /* ungrab all */
  g_free (data);
  return 0;
}

/*    vim: sw=3 ts=8 cindent noai bs=2 cinoptions=(0
 */
