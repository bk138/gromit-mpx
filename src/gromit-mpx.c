/* 
 * Gromit-MPX -- a program for painting on the screen
 *
 * Gromit Copyright (C) 2000 Simon Budig <Simon.Budig@unix-ag.org>
 *
 * Gromit-MPX Copyright (C) 2009,2010 Christian Beier <dontmind@freeshell.org>
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

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <X11/extensions/XInput2.h>

#include "callbacks.h"
#include "config.h"
#include "input.h"
#include "gromit-mpx.h"

#include "paint_cursor.xpm"
#include "erase_cursor.xpm"



GromitPaintContext *paint_context_new (GromitData *data, 
				       GromitPaintType type,
				       GdkColor *paint_color, 
				       guint width,
				       guint arrowsize)
{
  GromitPaintContext *context;

  context = g_malloc (sizeof (GromitPaintContext));

  context->type = type;
  context->width = width;
  context->arrowsize = arrowsize;
  context->paint_color = paint_color;

  
  context->paint_ctx = cairo_create (data->backbuffer);

  gdk_cairo_set_source_color(context->paint_ctx, paint_color);
  if(!data->composited)
    cairo_set_antialias(context->paint_ctx, CAIRO_ANTIALIAS_NONE);
  cairo_set_line_width(context->paint_ctx, width);
  cairo_set_line_cap(context->paint_ctx, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_join(context->paint_ctx, CAIRO_LINE_JOIN_ROUND);
  
  if (type == GROMIT_ERASER)
    cairo_set_operator(context->paint_ctx, CAIRO_OPERATOR_CLEAR);
  else
    if (type == GROMIT_RECOLOR)
      cairo_set_operator(context->paint_ctx, CAIRO_OPERATOR_ATOP);
    else /* GROMIT_PEN */
      cairo_set_operator(context->paint_ctx, CAIRO_OPERATOR_OVER);

  return context;
}


void paint_context_print (gchar *name, 
			  GromitPaintContext *context)
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
  g_printerr ("color: #%02X%02X%02X\n", context->paint_color->red >> 8,
              context->paint_color->green >> 8, context->paint_color->blue >> 8);
}


void paint_context_free (GromitPaintContext *context)
{
  cairo_destroy(context->paint_ctx);
  g_free (context);
}


void coord_list_prepend (GromitData *data, 
			 GdkDevice* dev, 
			 gint x, 
			 gint y, 
			 gint width)
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


void coord_list_free (GromitData *data, 
		      GdkDevice* dev)
{
  /* get the data for this device */
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


gboolean coord_list_get_arrow_param (GromitData *data,
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


void hide_window (GromitData *data)
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
      release_grab (data, NULL); /* release all */
      gtk_widget_hide (data->win);
      
      if(data->debug)
        g_printerr ("DEBUG: Hiding window.\n");
    }
}


void show_window (GromitData *data)
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
            acquire_grab (data, devdata->device);
        }
      if(data->debug)
        g_printerr ("DEBUG: Showing window.\n");
    }
  gdk_window_raise (gtk_widget_get_window(data->win));
}



void toggle_visibility (GromitData *data)
{
  if (data->hidden)
    show_window (data);
  else
    hide_window (data);
}



void clear_screen (GromitData *data)
{
  cairo_t *cr = cairo_create(data->backbuffer);
  cairo_set_source_rgba(cr, 0, 0, 0, 0);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint (cr);
  cairo_destroy(cr);

  GdkRectangle rect = {0, 0, data->width, data->height};
  gdk_window_invalidate_rect(gtk_widget_get_window(data->win), &rect, 0); 
  
  if(!data->composited)
    {
      cairo_region_t* r = gdk_cairo_region_create_from_surface(data->backbuffer);
      gtk_widget_shape_combine_region(data->win, r);
      cairo_region_destroy(r);
      // try to set transparent for input
      r =  cairo_region_create();
      gtk_widget_input_shape_combine_region(data->win, r);
      cairo_region_destroy(r);
    }

  data->painted = 0;

  if(data->debug)
    g_printerr ("DEBUG: Cleared screen.\n");
}


gint reshape (gpointer user_data)
{
  GromitData *data = (GromitData *) user_data;

  if (data->modified && !data->composited)
    {
      if (gtk_events_pending () && data->delayed < 5)
        {
          data->delayed++ ;
        }
      else
        {
	  cairo_region_t* r = gdk_cairo_region_create_from_surface(data->backbuffer);
	  gtk_widget_shape_combine_region(data->win, r);
	  cairo_region_destroy(r);
	  // try to set transparent for input
	  r =  cairo_region_create();
	  gtk_widget_input_shape_combine_region(data->win, r);
	  cairo_region_destroy(r);

          data->modified = 0;
          data->delayed = 0;
        }
    }
  return 1;
}


void select_tool (GromitData *data, 
		  GdkDevice *master, 
		  GdkDevice *slave, 
		  guint state)
{
  guint buttons = 0, modifier = 0, len = 0, default_len = 0;
  guint req_buttons = 0, req_modifier = 0;
  guint i, j, success = 0;
  GromitPaintContext *context = NULL;
  guchar *name;
  guchar *default_name;

  /* get the data for this device */
  GromitDeviceData *masterdata =
    g_hash_table_lookup(data->devdatatable, master);
  GromitDeviceData *slavedata =
    g_hash_table_lookup(data->devdatatable, slave);
 
  if (slave)
    {
      len = strlen (gdk_device_get_name(slave));
      name = (guchar*) g_strndup (gdk_device_get_name(slave), len + 3);
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

	      if(data->debug)
		g_printerr("DEBUG: Looking up context %s\n", name);

              context = g_hash_table_lookup (data->tool_config, name);
              if(context)
                {
                  if(data->debug)
                    g_printerr("DEBUG: Context %s set\n", name);
                  slavedata->cur_context = context;
                  success = 1;
                }
              else /* try default_name */
                if((context = g_hash_table_lookup (data->tool_config, default_name)))
                  {
                    if(data->debug)
                      g_printerr("DEBUG: Default context %s set\n", default_name);
                    slavedata->cur_context = context;
                    success = 1;
                  }
                
            }
          while (j<=3 && req_modifier >= (1u << j));
        }
      while (i<=5 && req_buttons >= (1u << i));

      g_free (name);
      g_free (default_name);

      if (!success)
        {
          if (gdk_device_get_source(slave) == GDK_SOURCE_ERASER)
            slavedata->cur_context = data->default_eraser;
          else
            slavedata->cur_context = data->default_pen;
        }
    }
  else
    g_printerr ("ERROR: Attempt to select nonexistent device!\n");

  GdkCursor *cursor;
  if(slavedata->cur_context && slavedata->cur_context->type == GROMIT_ERASER)
    cursor = data->erase_cursor;
  else
    cursor = data->paint_cursor;


  if(data->debug)
    g_printerr("DEBUG: Setting cursor %p\n",cursor);


  //FIXME!  Should be:
  //gdk_window_set_cursor(gtk_widget_get_window(data->win), cursor);
  // doesn't work during a grab?
  gdk_device_grab(master,
  		  gtk_widget_get_window(data->win),
  		  GDK_OWNERSHIP_NONE,
  		  FALSE,
  		  GROMIT_MOUSE_EVENTS,
  		  cursor,
  		  GDK_CURRENT_TIME);

  masterdata->lastslave = slave;
  masterdata->state = state;
  slavedata->state = state;
}



void snap_undo_state (GromitData *data)
{
  /* Copy from backbuffer -> undobuffer */
  cairo_t *cr = cairo_create(data->undobuffer);
  cairo_set_source_surface(cr, data->backbuffer, 0, 0);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint (cr);
  cairo_destroy(cr);

  if(data->debug)
    g_printerr ("DEBUG: Snapped undo buffer.\n");
}



void undo_drawing (GromitData *data)
{
  /* Swap data->backbuffer and data->undobuffer */
  cairo_surface_t *temp = data->backbuffer;
  data->backbuffer = data->undobuffer;
  data->undobuffer = temp;

  GdkRectangle rect = {0, 0, data->width, data->height};
  gdk_window_invalidate_rect(gtk_widget_get_window(data->win), &rect, 0); 

  if(data->debug)
    g_printerr ("DEBUG: Undo drawing.\n");
}



void redo_drawing (GromitData *data)
{
  if(data->debug)
    g_printerr("DEBUG: Redo drawing (same as undo currently).\n");
  undo_drawing (data);
}



void draw_line (GromitData *data,
		GdkDevice *dev,
		gint x1, gint y1,
		gint x2, gint y2)
{
  GdkRectangle rect;
  GromitDeviceData *devdata = g_hash_table_lookup(data->devdatatable, dev);

  rect.x = MIN (x1,x2) - data->maxwidth / 2;
  rect.y = MIN (y1,y2) - data->maxwidth / 2;
  rect.width = ABS (x1-x2) + data->maxwidth;
  rect.height = ABS (y1-y2) + data->maxwidth;

  if(data->debug)
    g_printerr("DEBUG: draw line from %d %d to %d %d\n", x1, y1, x2, y2);

  if (devdata->cur_context->paint_ctx)
    {
      cairo_set_line_width(devdata->cur_context->paint_ctx, data->maxwidth);
      cairo_set_line_cap(devdata->cur_context->paint_ctx, CAIRO_LINE_CAP_ROUND);
      cairo_set_line_join(devdata->cur_context->paint_ctx, CAIRO_LINE_JOIN_ROUND);
 
      cairo_move_to(devdata->cur_context->paint_ctx, x1, y1);
      cairo_line_to(devdata->cur_context->paint_ctx, x2, y2);
      cairo_stroke(devdata->cur_context->paint_ctx);

      data->modified = 1;

      gdk_window_invalidate_rect(gtk_widget_get_window(data->win), &rect, 0); 
    }

  data->painted = 1;
}


void draw_arrow (GromitData *data, 
		 GdkDevice *dev,
		 gint x1, gint y1,
		 gint width,
		 gfloat direction)
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

  if (devdata->cur_context->paint_ctx)
    {
      cairo_set_line_width(devdata->cur_context->paint_ctx, 1);
      cairo_set_line_cap(devdata->cur_context->paint_ctx, CAIRO_LINE_CAP_ROUND);
      cairo_set_line_join(devdata->cur_context->paint_ctx, CAIRO_LINE_JOIN_ROUND);
 
      cairo_move_to(devdata->cur_context->paint_ctx, arrowhead[0].x, arrowhead[0].y);
      cairo_line_to(devdata->cur_context->paint_ctx, arrowhead[1].x, arrowhead[1].y);
      cairo_line_to(devdata->cur_context->paint_ctx, arrowhead[2].x, arrowhead[2].y);
      cairo_line_to(devdata->cur_context->paint_ctx, arrowhead[3].x, arrowhead[3].y);
      cairo_fill(devdata->cur_context->paint_ctx);

      gdk_cairo_set_source_color(devdata->cur_context->paint_ctx, data->black);

      cairo_move_to(devdata->cur_context->paint_ctx, arrowhead[0].x, arrowhead[0].y);
      cairo_line_to(devdata->cur_context->paint_ctx, arrowhead[1].x, arrowhead[1].y);
      cairo_line_to(devdata->cur_context->paint_ctx, arrowhead[2].x, arrowhead[2].y);
      cairo_line_to(devdata->cur_context->paint_ctx, arrowhead[3].x, arrowhead[3].y);
      cairo_line_to(devdata->cur_context->paint_ctx, arrowhead[0].x, arrowhead[0].y);
      cairo_stroke(devdata->cur_context->paint_ctx);

      gdk_cairo_set_source_color(devdata->cur_context->paint_ctx, devdata->cur_context->paint_color);
    
      data->modified = 1;

      gdk_window_invalidate_rect(gtk_widget_get_window(data->win), &rect, 0); 
    }

  data->painted = 1;
}




/*
 * Functions for handling various (GTK+)-Events
 */


/* Keyboard control */

gint key_press_event (GtkWidget   *grab_widget,
		      GdkEventKey *event,
		      gpointer     func_data)
{
  GromitData *data = (GromitData *) func_data;
  GdkDevice *dev = gdk_event_get_device((GdkEvent*)event);

  if (event->type == GDK_KEY_PRESS &&
      event->hardware_keycode == data->hot_keycode)
    {
      if(data->debug)
	g_printerr("DEBUG: Received hotkey press from device '%s'\n", gdk_device_get_name(dev));

      if (event->state & GDK_SHIFT_MASK)
        clear_screen (data);
      else if (event->state & GDK_CONTROL_MASK)
        toggle_visibility (data);
      else if (event->state & GDK_MOD1_MASK)
        gtk_main_quit ();
      else
	{
	  /* GAAAH */
	  gint kbd_dev_id = -1;
	  g_object_get(dev, "device-id", &kbd_dev_id, NULL);

	  XIDeviceInfo* devinfo;
	  int devicecount = 0;
	  gint ptr_dev_id = -1;
	  
	  devinfo = XIQueryDevice(GDK_DISPLAY_XDISPLAY(data->display),
				  kbd_dev_id,
				  &devicecount);
	  if(devicecount)
	    ptr_dev_id = devinfo->attachment;
	  XIFreeDeviceInfo(devinfo);
	  
 
	  GdkDeviceManager *device_manager = gdk_display_get_device_manager(data->display);
	  GList *devices, *d;
	  gint some_dev_id;
	  GdkDevice *some_device = NULL;
	  devices = gdk_device_manager_list_devices(device_manager, GDK_DEVICE_TYPE_MASTER);
	  for(d = devices; d; d = d->next)
	    {
	      some_device = (GdkDevice *) d->data;
	      g_object_get(some_device, "device-id", &some_dev_id, NULL);
	      if(some_dev_id == ptr_dev_id)
		break;
	    }


	  toggle_grab(data, some_device);
	}

      return TRUE;
    }
  return FALSE;
}


void main_do_event (GdkEventAny *event,
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
      event->window = gtk_widget_get_window(data->win);
      g_object_ref (gtk_widget_get_window(data->win));
    }

  gtk_main_do_event ((GdkEvent *) event);
}





void setup_main_app (GromitData *data, gboolean activate)
{
  /* COLOURS */
  g_free(data->white);
  g_free(data->black);
  g_free(data->red);
  data->white = g_malloc (sizeof (GdkColor));
  data->black = g_malloc (sizeof (GdkColor));
  data->red   = g_malloc (sizeof (GdkColor));
  gdk_color_parse ("#FFFFFF", data->white);
  gdk_color_parse ("#000000", data->black);
  gdk_color_parse ("#FF0000", data->red);


  /* 
     CURSORS
  */
  GdkPixbuf* paint_cursor_pixbuf = gdk_pixbuf_new_from_xpm_data(paint_cursor_xpm);
  data->paint_cursor = gdk_cursor_new_from_pixbuf(data->display,
						  paint_cursor_pixbuf,
						  paint_cursor_x_hot,
						  paint_cursor_y_hot);
  g_object_unref (paint_cursor_pixbuf);

  GdkPixbuf* erase_cursor_pixbuf = gdk_pixbuf_new_from_xpm_data(erase_cursor_xpm);
  data->erase_cursor = gdk_cursor_new_from_pixbuf(data->display,
						  erase_cursor_pixbuf,
						  erase_cursor_x_hot,
						  erase_cursor_y_hot);
  g_object_unref (erase_cursor_pixbuf);


  /*
    DRAWING AREA
  */
  /* SHAPE SURFACE*/
  cairo_surface_destroy(data->backbuffer);
  data->backbuffer = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, data->width, data->height);
  cairo_surface_destroy(data->undobuffer);
  data->undobuffer = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, data->width, data->height);


  /* EVENTS */
  gtk_widget_add_events (data->win, GROMIT_WINDOW_EVENTS);
  g_signal_connect (data->win, "draw",
		    G_CALLBACK (on_expose), data);
  g_signal_connect (data->win,"configure_event",
		    G_CALLBACK (on_configure), data);
  g_signal_connect (data->win,"screen-changed",
		    G_CALLBACK (on_screen_changed), data);
  g_signal_connect (data->screen,"monitors_changed",
		    G_CALLBACK (on_monitors_changed), data);
  g_signal_connect (data->screen,"composited-changed",
		    G_CALLBACK (on_composited_changed), data);
  g_signal_connect (gdk_display_get_device_manager (data->display), "device-added",
                    G_CALLBACK (on_device_added), data);
  g_signal_connect (gdk_display_get_device_manager (data->display), "device-removed",
                    G_CALLBACK (on_device_removed), data);
  g_signal_connect (data->win, "motion_notify_event",
		    G_CALLBACK (on_motion), data);
  g_signal_connect (data->win, "button_press_event", 
		    G_CALLBACK (on_buttonpress), data);
  g_signal_connect (data->win, "button_release_event",
		    G_CALLBACK (on_buttonrelease), data);
  /* disconnect previously defined selection handlers */
  g_signal_handlers_disconnect_by_func (data->win, 
					G_CALLBACK (on_clientapp_selection_get),
					data);
  g_signal_handlers_disconnect_by_func (data->win, 
					G_CALLBACK (on_clientapp_selection_received),
					data);
  /* and re-connect them to mainapp functions */
  g_signal_connect (data->win, "selection_get",
		    G_CALLBACK (on_mainapp_selection_get), data);
  g_signal_connect (data->win, "selection_received",
		    G_CALLBACK (on_mainapp_selection_received), data);


  if(!data->composited) // set initial shape
    {
      cairo_region_t* r = gdk_cairo_region_create_from_surface(data->backbuffer);
      gtk_widget_shape_combine_region(data->win, r);
      cairo_region_destroy(r);
    }
  

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
  gtk_selection_add_target (data->win, GA_CONTROL, GA_UNDO, 8);
  gtk_selection_add_target (data->win, GA_CONTROL, GA_REDO, 9);


 

  /*
   * Parse Config file
   */
  data->tool_config = g_hash_table_new (g_str_hash, g_str_equal);
  parse_config (data);
  if (data->debug)
    g_hash_table_foreach (data->tool_config, parse_print_help, NULL);
  

  /* 
     FIND HOTKEY KEYCODE 
  */
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

          data->hot_keycode = keys[0].keycode;
          g_free (keys);
        }
    }


  /* 
     INPUT DEVICES
  */
  data->devdatatable = g_hash_table_new(NULL, NULL);
  setup_input_devices (data);



  gtk_widget_show_all (data->win);

  data->painted = 0;
  hide_window (data);

  data->timeout_id = g_timeout_add (20, reshape, data);
 
  data->modified = 0;

  data->default_pen = paint_context_new (data, GROMIT_PEN,
					 data->red, 7, 0);
  data->default_eraser = paint_context_new (data, GROMIT_ERASER,
					    data->red, 75, 0);

  

  gdk_event_handler_set ((GdkEventFunc) main_do_event, data, NULL);
  gtk_key_snooper_install (key_press_event, data);

  if (activate)
    acquire_grab (data, NULL); /* grab all */

  /* 
     TRAY ICON
  */
  data->trayicon = gtk_status_icon_new_from_file("/usr/share/pixmaps/gromit-mpx.png");
  gtk_status_icon_set_tooltip_text (data->trayicon, "Gromit-MPX");
  g_signal_connect (data->trayicon, "activate",
		    G_CALLBACK (on_trayicon_activate), data);
  g_signal_connect (data->trayicon, "popup-menu",
		    G_CALLBACK (on_trayicon_menu), data);

}

void parse_print_help (gpointer key, gpointer value, gpointer user_data)
{
  paint_context_print ((gchar *) key, (GromitPaintContext *) value);
}


int app_parse_args (int argc, char **argv, GromitData *data)
{
   gint      i;
   gchar    *arg;
   gboolean  wrong_arg = FALSE;
   gboolean  activate = FALSE;

   data->hot_keyval = DEFAULT_HOTKEY;
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
           g_printerr ("Unknown Option for Gromit-MPX startup: \"%s\"\n", arg);
           wrong_arg = TRUE;
         }

       if (wrong_arg)
         {
           g_printerr ("Please see the Gromit-MPX manpage for the correct usage\n");
           exit (1);
         }
     }

   return activate;
}


/*
 * Main programs
 */

int main_client (int argc, char **argv, GromitData *data)
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
       else if (strcmp (arg, "-z") == 0 ||
                strcmp (arg, "--undo") == 0)
         {
           action = GA_UNDO;
         }
       else if (strcmp (arg, "-y") == 0 ||
                strcmp (arg, "--redo") == 0)
         {
           action = GA_REDO;
         }
       else
         {
           g_printerr ("Unknown Option to control a running Gromit-MPX process: \"%s\"\n", arg);
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
           g_printerr ("Please see the Gromit-MPX manpage for the correct usage\n");
           return 1;
         }
     }


   return 0;
}

int main (int argc, char **argv)
{
  GromitData *data;

  gtk_init (&argc, &argv);
  data = g_malloc0(sizeof (GromitData));

  /* 
     init basic stuff
  */
  data->display = gdk_display_get_default ();
  data->screen = gdk_display_get_default_screen (data->display);
  data->xinerama = gdk_screen_get_n_monitors (data->screen) > 1;
  data->composited = gdk_screen_is_composited (data->screen);
  data->root = gdk_screen_get_root_window (data->screen);
  data->width = gdk_screen_get_width (data->screen);
  data->height = gdk_screen_get_height (data->screen);

  /*
    init our window
  */
  data->win = gtk_window_new (GTK_WINDOW_POPUP);
  // this trys to set an alpha channel
  on_screen_changed(data->win, NULL, data);

  gtk_window_fullscreen(GTK_WINDOW(data->win)); 
  gtk_window_set_skip_taskbar_hint(GTK_WINDOW(data->win), TRUE);
  gtk_window_set_opacity(GTK_WINDOW(data->win), 0.75);
  gtk_widget_set_app_paintable (data->win, TRUE);
  gtk_window_set_decorated (GTK_WINDOW (data->win), FALSE);

  gtk_widget_set_size_request (GTK_WIDGET (data->win), data->width, data->height-1);

  gtk_widget_add_events (data->win, GROMIT_WINDOW_EVENTS);

  g_signal_connect (data->win, "delete-event", gtk_main_quit, NULL);
  g_signal_connect (data->win, "destroy", gtk_main_quit, NULL);
  /* the selection event handlers will be overwritten if we become a mainapp */
  g_signal_connect (data->win, "selection_received", 
		    G_CALLBACK (on_clientapp_selection_received), data);
  g_signal_connect (data->win, "selection_get",
		    G_CALLBACK (on_clientapp_selection_get), data);
  
  gtk_widget_realize (data->win);

  // try to set transparent for input
  cairo_region_t* r =  cairo_region_create();
  gtk_widget_input_shape_combine_region(data->win, r);
  cairo_region_destroy(r);

  gtk_selection_owner_set (data->win, GA_DATA, GDK_CURRENT_TIME);
  gtk_selection_add_target (data->win, GA_DATA, GA_TOGGLEDATA, 1007);



  /* Try to get a status message. If there is a response gromit
   * is already active.
   */

  gtk_selection_convert (data->win, GA_CONTROL, GA_STATUS,
                         GDK_CURRENT_TIME);
  gtk_main ();  /* Wait for the response */

  if (data->client)
    return main_client (argc, argv, data);

  /* Main application */
  setup_main_app (data, app_parse_args (argc, argv, data));
  gtk_main ();
  release_grab(data, NULL); /* ungrab all */
  g_free (data);
  return 0;
}

