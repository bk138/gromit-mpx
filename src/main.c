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
#include <stdlib.h>
#include <lz4.h>

#include "callbacks.h"
#include "config.h"
#include "input.h"
#include "main.h"
#include "build-config.h"

#include "paint_cursor.xpm"
#include "erase_cursor.xpm"
#include "coordlist_ops.h"



GromitPaintContext *paint_context_new (GromitData *data,
				       GromitPaintType type,
				       GdkRGBA *paint_color,
				       guint width,
				       guint arrowsize,
                                       GromitArrowType arrowtype,
                                       guint simpilfy,
                                       guint radius,
                                       guint maxangle,
                                       guint minlen,
                                       guint snapdist,
				       guint minwidth,
				       guint maxwidth)
{
  GromitPaintContext *context;

  context = g_malloc (sizeof (GromitPaintContext));

  context->type = type;
  context->width = width;
  context->arrowsize = arrowsize;
  context->arrow_type = arrowtype;
  context->minwidth = minwidth;
  context->maxwidth = maxwidth;
  context->paint_color = paint_color;
  context->radius = radius;
  context->maxangle = maxangle;
  context->simplify = simpilfy;
  context->minlen = minlen;
  context->snapdist = snapdist;

  context->paint_ctx = cairo_create (data->backbuffer);

  gdk_cairo_set_source_rgba(context->paint_ctx, paint_color);
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
      g_printerr ("Pen,        "); break;
    case GROMIT_LINE:
      g_printerr ("Line,       "); break;
    case GROMIT_RECT:
      g_printerr ("Rect,       "); break;
    case GROMIT_SMOOTH:
      g_printerr ("Smooth,     "); break;
    case GROMIT_ORTHOGONAL:
      g_printerr ("Orthogonal, "); break;
    case GROMIT_ERASER:
      g_printerr ("Eraser,     "); break;
    case GROMIT_RECOLOR:
      g_printerr ("Recolor,    "); break;
    default:
      g_printerr ("UNKNOWN,    "); break;
  }

  g_printerr ("width: %u, ", context->width);
  g_printerr ("minwidth: %u, ", context->minwidth);
  g_printerr ("maxwidth: %u, ", context->maxwidth);
  g_printerr ("arrowsize: %.2f, ", context->arrowsize);
  if (context->arrowsize > 0)
    {
      switch (context->arrow_type) {
      case GROMIT_ARROW_START:
        g_printerr(" arrowtype: start,  ");
        break;
      case GROMIT_ARROW_END:
        g_printerr(" arrowtype: end,    ");
        break;
      case GROMIT_ARROW_DOUBLE:
        g_printerr(" arrowtype: double, ");
        break;
      }
    }
  if (context->type == GROMIT_SMOOTH || context->type == GROMIT_ORTHOGONAL)
    {
      g_printerr(" simplify: %u, ", context->simplify);
      if (context->snapdist > 0)
        g_printerr(" snap: %u, ", context->snapdist);
    }
  if (context->type == GROMIT_ORTHOGONAL)
    {
      g_printerr(" radius: %u, minlen: %u, maxangle: %u ",
                 context->radius, context->minlen, context->maxangle);
    }
  g_printerr ("color: %s\n", gdk_rgba_to_string(context->paint_color));
}


void paint_context_free (GromitPaintContext *context)
{
  cairo_destroy(context->paint_ctx);
  g_free (context);
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

	  /*
	    this is not needed when there is user input, i.e. when drawing,
	    but needed when uing the undo functionality.
	  */
	  gtk_widget_queue_draw(data->win);

          data->modified = 0;
          data->delayed = 0;
        }
    }
  return 1;
}


void select_tool (GromitData *data,
		  GdkDevice *device,
		  GdkDevice *slave_device,
		  guint state)
{
  guint buttons = 0, modifier = 0, slave_len = 0, len = 0, default_len = 0;
  guint req_buttons = 0, req_modifier = 0;
  guint i, j, success = 0;
  GromitPaintContext *context = NULL;
  guchar *slave_name;
  guchar *name;
  guchar *default_name;

  /* get the data for this device */
  GromitDeviceData *devdata = g_hash_table_lookup(data->devdatatable, device);

  if (device)
    {
      slave_len = strlen (gdk_device_get_name(slave_device));
      slave_name = (guchar*) g_strndup (gdk_device_get_name(slave_device), slave_len + 3);
      len = strlen (gdk_device_get_name(device));
      name = (guchar*) g_strndup (gdk_device_get_name(device), len + 3);
      default_len = strlen(DEFAULT_DEVICE_NAME);
      default_name = (guchar*) g_strndup (DEFAULT_DEVICE_NAME, default_len + 3);


      /* Extract Button/Modifiers from state (see GdkModifierType) */
      req_buttons = (state >> 8) & 31;

      req_modifier = (state >> 1) & 7;
      if (state & GDK_SHIFT_MASK) req_modifier |= 1;

      slave_name [slave_len] = 124;
      slave_name [slave_len+3] = 0;
      name [len] = 124;
      name [len+3] = 0;
      default_name [default_len] = 124;
      default_name [default_len+3] = 0;

      /*
	Iterate i up until <= req_buttons.
	For each i, find out if bits of i are _all_ in `req_buttons`.
	- If yes, lookup if there is tool and select if there is.
	- If no, try next i, no tool lookup.
      */
      context = NULL;
      i=-1;
      do
        {
          i++;

	  /*
	    For all i > 0, find out if _all_ bits representing the iterator 'i'
	    are present in req_buttons as well. If not (none or only some are),
	    then go on.
	    The condition i==0 handles the config cases where no button is given.
	  */
	  buttons = i & req_buttons;
	  if(i > 0 && (buttons == 0 || buttons != i))
	      continue;

          j=-1;
          do
            {
              j++;
              modifier = req_modifier & ((1 << j)-1);
              slave_name [slave_len+1] = buttons + 64;
              slave_name [slave_len+2] = modifier + 48;
              name [len+1] = buttons + 64;
              name [len+2] = modifier + 48;
              default_name [default_len+1] = buttons + 64;
              default_name [default_len+2] = modifier + 48;

	            if(data->debug)
                g_printerr("DEBUG: select_tool looking up context for '%s' attached to '%s'\n", slave_name, name);

              context = g_hash_table_lookup (data->tool_config, slave_name);
              if(context) {
                  if(data->debug)
                    g_printerr("DEBUG: select_tool set context for '%s'\n", slave_name);
                  devdata->cur_context = context;
                  success = 1;
              }
              else /* try master name */
              if ((context = g_hash_table_lookup (data->tool_config, name)))
                {
                  if(data->debug)
                    g_printerr("DEBUG: select_tool set context for '%s'\n", name);
                  devdata->cur_context = context;
                  success = 1;
                }
              else /* try default_name */
                if((context = g_hash_table_lookup (data->tool_config, default_name)))
                  {
                    if(data->debug)
                      g_printerr("DEBUG: select_tool set default context '%s' for '%s'\n", default_name, name);
                    devdata->cur_context = context;
                    success = 1;
                  }

            }
          while (j<=3 && req_modifier >= (1u << j));
        }
      while (i < req_buttons);

      if (!success)
        {
          if (gdk_device_get_source(device) == GDK_SOURCE_ERASER)
            devdata->cur_context = data->default_eraser;
          else
            devdata->cur_context = data->default_pen;

	  if(data->debug)
	      g_printerr("DEBUG: select_tool set fallback context for '%s'\n", name);
        }

      g_free (name);
      g_free (default_name);
    }
  else
    g_printerr ("ERROR: select_tool attempted to select nonexistent device!\n");

  GdkCursor *cursor;
  if(devdata->cur_context && devdata->cur_context->type == GROMIT_ERASER)
    cursor = data->erase_cursor;
  else
    cursor = data->paint_cursor;


  if(data->debug)
    g_printerr("DEBUG: select_tool setting cursor %p\n",cursor);


  //FIXME!  Should be:
  //gdk_window_set_cursor(gtk_widget_get_window(data->win), cursor);
  // doesn't work during a grab?
  gdk_device_grab(device,
  		  gtk_widget_get_window(data->win),
  		  GDK_OWNERSHIP_NONE,
  		  FALSE,
  		  GROMIT_MOUSE_EVENTS,
  		  cursor,
  		  GDK_CURRENT_TIME);

  devdata->state = state;
  devdata->lastslave = slave_device;
}



void snap_undo_state (GromitData *data)
{
  if(data->debug)
    g_printerr ("DEBUG: Snapping undo buffer %d.\n", data->undo_head);

  undo_compress(data, data->backbuffer);
  undo_temp_buffer_to_slot(data, data->undo_head);

  // Increment head position
  data->undo_head++;
  if(data->undo_head >= GROMIT_MAX_UNDO)
    data->undo_head -= GROMIT_MAX_UNDO;
  data->undo_depth++;
  // See if we ran out of undo levels with oldest undo overwritten
  if(data->undo_depth > GROMIT_MAX_UNDO)
    data->undo_depth = GROMIT_MAX_UNDO;
  // Invalidate any redo from this position
  data->redo_depth = 0;
}



void copy_surface (cairo_surface_t *dst, cairo_surface_t *src)
{
  cairo_t *cr = cairo_create(dst);
  cairo_set_source_surface(cr, src, 0, 0);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint (cr);
  cairo_destroy(cr);
}


void undo_drawing (GromitData *data)
{
  if(data->undo_depth <= 0)
    return;
  data->undo_depth--;
  data->redo_depth++;
  if(data->redo_depth > GROMIT_MAX_UNDO)
    data->redo_depth -= GROMIT_MAX_UNDO;
  data->undo_head--;
  if(data->undo_head < 0)
    data->undo_head += GROMIT_MAX_UNDO;

  undo_compress(data, data->backbuffer);
  undo_decompress(data, data->undo_head, data->backbuffer);
  undo_temp_buffer_to_slot(data, data->undo_head);

  GdkRectangle rect = {0, 0, data->width, data->height};
  gdk_window_invalidate_rect(gtk_widget_get_window(data->win), &rect, 0);

  data->modified = 1;

  if(data->debug)
    g_printerr ("DEBUG: Undo drawing %d.\n", data->undo_head);
}



void redo_drawing (GromitData *data)
{
  if(data->redo_depth <= 0)
    return;

  undo_compress(data, data->backbuffer);
  undo_decompress(data, data->undo_head, data->backbuffer);
  undo_temp_buffer_to_slot(data, data->undo_head);

  data->redo_depth--;
  data->undo_depth++;
  data->undo_head++;
  if(data->undo_head >= GROMIT_MAX_UNDO)
    data->undo_head -= GROMIT_MAX_UNDO;

  GdkRectangle rect = {0, 0, data->width, data->height};
  gdk_window_invalidate_rect(gtk_widget_get_window(data->win), &rect, 0);

  data->modified = 1;
  
  if(data->debug)
    g_printerr("DEBUG: Redo drawing.\n");
}

/*
 * compress image data and store it in undo_temp_buffer
 *
 * the undo_temp_buffer is successively grown in case it is too small
 */
void undo_compress(GromitData *data, cairo_surface_t *surface)
{
  char *raw_data = (char *)cairo_image_surface_get_data(surface);
  guint bytes_per_row = cairo_image_surface_get_stride(surface);
  guint rows = cairo_image_surface_get_height(surface);
  size_t src_bytes = rows * bytes_per_row;

  size_t dest_bytes;
  for (;;)
    {
      dest_bytes = LZ4_compress_default(raw_data, data->undo_temp, src_bytes, data->undo_temp_size);
      if (dest_bytes == 0)
        {
          data->undo_temp_size *= 2;
          data->undo_temp = g_realloc(data->undo_temp, data->undo_temp_size);
        }
      else
        {
          data->undo_temp_used = dest_bytes;
          break;
        }
    }
}


/*
 * copy undo_temp to undo slot, growing undo buffer if required
 */
void undo_temp_buffer_to_slot(GromitData *data, gint undo_slot)
{
    const size_t required = data->undo_temp_used;
    if (data->undo_buffer_size[undo_slot] < required)
      {
        data->undo_buffer[undo_slot] = g_realloc(data->undo_buffer[undo_slot], required);
        data->undo_buffer_size[undo_slot] = required;
      }
    data->undo_buffer_used[undo_slot] = required;
    memcpy(data->undo_buffer[undo_slot], data->undo_temp, required);
}

/*
 * decompress undo slot data and store it in cairo surface
 */
void undo_decompress(GromitData *data, gint undo_slot, cairo_surface_t *surface)
{
  char *dest_data = (char *)cairo_image_surface_get_data(surface);
  guint bytes_per_row = cairo_image_surface_get_stride(surface);
  guint rows = cairo_image_surface_get_height(surface);
  size_t dest_bytes = rows * bytes_per_row;

  char *src_data = data->undo_buffer[undo_slot];
  size_t src_bytes = data->undo_buffer_used[undo_slot];

  if (LZ4_decompress_safe(src_data, dest_data, src_bytes, dest_bytes) < 0) {
    g_printerr("Fatal error occurred decompressing image data\n");
    exit(1);
  }
}

/*
 * Functions for handling various (GTK+)-Events
 */


void main_do_event (GdkEventAny *event,
		    GromitData  *data)
{
  guint keycode = ((GdkEventKey *) event)->hardware_keycode;
  if ((event->type == GDK_KEY_PRESS ||
       event->type == GDK_KEY_RELEASE) &&
      event->window == data->root &&
      (keycode == data->hot_keycode || keycode == data->undo_keycode))
    {
      /* redirect the event to our main window, so that GTK+ doesn't
       * throw it away (there is no GtkWidget for the root window...)
       */
      event->window = gtk_widget_get_window(data->win);
      g_object_ref (gtk_widget_get_window(data->win));
    }

  gtk_main_do_event ((GdkEvent *) event);
}





void setup_main_app (GromitData *data, int argc, char ** argv)
{
  gboolean activate;

  if(getenv("GDK_CORE_DEVICE_EVENTS")) {
      g_printerr("GDK is set to not use the XInput extension, Gromit-MPX can not work this way.\n"
		 "Probably the GDK_CORE_DEVICE_EVENTS environment variable is set, try to start Gromit-MPX with this variable unset.\n");
      exit(1);
  }

  /*
    l18n
   */
  bindtextdomain(PACKAGE_LOCALE_DOMAIN, PACKAGE_LOCALE_DIR);
  textdomain(PACKAGE_LOCALE_DOMAIN);

  /*
    HOT KEYS
  */
  data->hot_keyval = DEFAULT_HOTKEY;
  data->hot_keycode = 0;

  data->undo_keyval = DEFAULT_UNDOKEY;
  data->undo_keycode = 0;

  char *xdg_current_desktop = getenv("XDG_CURRENT_DESKTOP");
  if (xdg_current_desktop && strcmp(xdg_current_desktop, "XFCE") == 0) {
      /*
	XFCE per default grabs Ctrl-F1 to Ctrl-F12 (switch to workspace 1-12)
	and Alt-F9 (minimize window) which renders Gromit-MPX's default hotkey
	mapping unusable. Provide different defaults for that desktop environment.
      */
      data->hot_keyval = DEFAULT_HOTKEY_XFCE;
      data->undo_keyval = DEFAULT_UNDOKEY_XFCE;
      g_print("Detected XFCE, changing default hot keys to '" DEFAULT_HOTKEY_XFCE "' and '" DEFAULT_UNDOKEY_XFCE "'\n");
  }

  /* COLOURS */
  g_free(data->white);
  g_free(data->black);
  g_free(data->red);
  data->white = g_malloc (sizeof (GdkRGBA));
  data->black = g_malloc (sizeof (GdkRGBA));
  data->red   = g_malloc (sizeof (GdkRGBA));
  gdk_rgba_parse(data->white, "#FFFFFF");
  gdk_rgba_parse(data->black, "#000000");
  gdk_rgba_parse(data->red, "#FF0000");


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

  // original state for LINE and RECT tool
  cairo_surface_destroy(data->aux_backbuffer);
  data->aux_backbuffer = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, data->width, data->height);

  /*
    UNDO STATE
  */
  data->undo_head = 0;
  data->undo_depth = 0;
  data->redo_depth = 0;
  data->undo_temp_size = 0x10000;
  data->undo_temp = g_malloc(data->undo_temp_size);
  data->undo_temp_used = 0;
  for (int i = 0; i < GROMIT_MAX_UNDO; i++)
    {
      data->undo_buffer_size[i] = 0;
      data->undo_buffer[i] = NULL;
    }

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
  gtk_selection_add_target (data->win, GA_CONTROL, GA_LINE, 10);




  /*
   * Parse Config file
   */
  data->tool_config = g_hash_table_new (g_str_hash, g_str_equal);
  parse_config (data);
  g_hash_table_foreach (data->tool_config, parse_print_help, NULL);

  /*
    parse key file
  */
  read_keyfile(data);

  /*
    parse cmdline
  */
  activate = parse_args (argc, argv, data);

  // might have been in key file
  gtk_widget_set_opacity(data->win, data->opacity);

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
     FIND UNDOKEY KEYCODE
  */
  if (data->undo_keyval)
    {
      GdkKeymap    *keymap;
      GdkKeymapKey *keys;
      gint          n_keys;
      guint         keyval;

      if (strlen (data->undo_keyval) > 0 &&
          strcasecmp (data->undo_keyval, "none") != 0)
        {
          keymap = gdk_keymap_get_for_display (data->display);
          keyval = gdk_keyval_from_name (data->undo_keyval);

          if (!keyval || !gdk_keymap_get_entries_for_keyval (keymap, keyval,
                                                             &keys, &n_keys))
            {
              g_printerr ("cannot find the key \"%s\"\n", data->undo_keyval);
              exit (1);
            }

          data->undo_keycode = keys[0].keycode;
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

  data->default_pen =
    paint_context_new (data, GROMIT_PEN, data->red, 7, 0, GROMIT_ARROW_END,
                       5, 10, 15, 25, 0, 1, G_MAXUINT);
  data->default_eraser =
    paint_context_new (data, GROMIT_ERASER, data->red, 75, 0, GROMIT_ARROW_END,
                       5, 10, 15, 25, 0, 1, G_MAXUINT);

  gdk_event_handler_set ((GdkEventFunc) main_do_event, data, NULL);
  gtk_key_snooper_install (snoop_key_press, data);

  if (activate)
    acquire_grab (data, NULL); /* grab all */

  /*
     TRAY ICON
  */
  data->trayicon = app_indicator_new (PACKAGE_NAME,
				      "net.christianbeier.Gromit-MPX",
				      APP_INDICATOR_CATEGORY_APPLICATION_STATUS);

  app_indicator_set_status (data->trayicon, APP_INDICATOR_STATUS_ACTIVE);



  /* create the menu */
  GtkWidget *menu = gtk_menu_new ();

  char labelBuf[128];
  /* Create the menu items */
  snprintf(labelBuf, sizeof(labelBuf), _("Toggle Painting (%s)"), data->hot_keyval);
  GtkWidget* toggle_paint_item = gtk_menu_item_new_with_label (labelBuf);
  snprintf(labelBuf, sizeof(labelBuf), _("Clear Screen (SHIFT-%s)"), data->hot_keyval);
  GtkWidget* clear_item = gtk_menu_item_new_with_label (labelBuf);
  snprintf(labelBuf, sizeof(labelBuf), _("Toggle Visibility (CTRL-%s)"), data->hot_keyval);
  GtkWidget* toggle_vis_item = gtk_menu_item_new_with_label (labelBuf);
  GtkWidget* thicker_lines_item = gtk_menu_item_new_with_label (_("Thicker Lines"));
  GtkWidget* thinner_lines_item = gtk_menu_item_new_with_label (_("Thinner Lines"));
  GtkWidget* opacity_bigger_item = gtk_menu_item_new_with_label (_("Bigger Opacity"));
  GtkWidget* opacity_lesser_item = gtk_menu_item_new_with_label (_("Lesser Opacity"));
  snprintf(labelBuf, sizeof(labelBuf), _("Undo (%s)"), data->undo_keyval);
  GtkWidget* undo_item = gtk_menu_item_new_with_label (labelBuf);
  snprintf(labelBuf, sizeof(labelBuf), _("Redo (SHIFT-%s)"), data->undo_keyval);
  GtkWidget* redo_item = gtk_menu_item_new_with_label (labelBuf);

  GtkWidget* sep1_item = gtk_separator_menu_item_new();
  GtkWidget* intro_item = gtk_menu_item_new_with_mnemonic(_("_Introduction"));
  GtkWidget* edit_config_item = gtk_menu_item_new_with_mnemonic(_("_Edit Config"));
  GtkWidget* issues_item = gtk_menu_item_new_with_mnemonic(_("_Report Bug / Request Feature"));
  GtkWidget* support_item = gtk_menu_item_new_with_mnemonic(_("_Support Gromit-MPX"));
  GtkWidget* about_item = gtk_menu_item_new_with_mnemonic(_("_About"));

  GtkWidget* sep2_item = gtk_separator_menu_item_new();
  snprintf(labelBuf, sizeof(labelBuf), _("_Quit (ALT-%s)"), data->hot_keyval);
  GtkWidget* quit_item = gtk_menu_item_new_with_mnemonic(labelBuf);


  /* Add them to the menu */
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), toggle_paint_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), clear_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), toggle_vis_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), thicker_lines_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), thinner_lines_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), opacity_bigger_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), opacity_lesser_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), undo_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), redo_item);

  gtk_menu_shell_append (GTK_MENU_SHELL (menu), sep1_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), intro_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), edit_config_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), issues_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), support_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), about_item);

  gtk_menu_shell_append (GTK_MENU_SHELL (menu), sep2_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), quit_item);


  /* Find out if the D-Bus name org.kde.StatusNotifierWatcher is owned. */
  GDBusConnection *dbus_conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
  GVariant *is_status_notifier_watcher_owned = g_dbus_connection_call_sync(dbus_conn,
						 "org.freedesktop.DBus",
						 "/org/freedesktop/DBus",
						 "org.freedesktop.DBus",
						 "GetConnectionUnixProcessID",
						 g_variant_new("(s)", "org.kde.StatusNotifierWatcher"),
						 G_VARIANT_TYPE("(u)"),
						 G_DBUS_CALL_FLAGS_NONE,
						 -1,
						 NULL,
						 NULL);
  if(data->debug)
      g_printerr("DEBUG: org.kde.StatusNotifierWatcher is %s\n", is_status_notifier_watcher_owned ? "owned" : "not owned");

  /* Attach the callback functions to the respective activate signal */
  if (is_status_notifier_watcher_owned) {
      // KStatusNotifier does not handle the device-specific "button-press-event" from a menu
      g_signal_connect(G_OBJECT (toggle_paint_item), "activate",
		       G_CALLBACK (on_toggle_paint_all),
		       data);
  } else {
      g_signal_connect(toggle_paint_item, "button-press-event",
		       G_CALLBACK(on_toggle_paint), data);
  }
  g_signal_connect(G_OBJECT (clear_item), "activate",
		   G_CALLBACK (on_clear),
		   data);
  g_signal_connect(G_OBJECT (toggle_vis_item), "activate",
		   G_CALLBACK (on_toggle_vis),
		   data);
  g_signal_connect(G_OBJECT (thicker_lines_item), "activate",
		   G_CALLBACK (on_thicker_lines),
		   data);
  g_signal_connect(G_OBJECT (thinner_lines_item), "activate",
		   G_CALLBACK (on_thinner_lines),
		   data);
  g_signal_connect(G_OBJECT (opacity_bigger_item), "activate",
		   G_CALLBACK (on_opacity_bigger),
		   data);
  g_signal_connect(G_OBJECT (opacity_lesser_item), "activate",
		   G_CALLBACK (on_opacity_lesser),
		   data);
  g_signal_connect(G_OBJECT (undo_item), "activate",
		   G_CALLBACK (on_undo),
		   data);
  g_signal_connect(G_OBJECT (redo_item), "activate",
		   G_CALLBACK (on_redo),
		   data);

  g_signal_connect(G_OBJECT (intro_item), "activate",
		   G_CALLBACK (on_intro),
		   data);
  g_signal_connect(G_OBJECT (edit_config_item), "activate",
		   G_CALLBACK (on_edit_config),
		   data);
  g_signal_connect(G_OBJECT (issues_item), "activate",
		   G_CALLBACK (on_issues),
		   data);
  g_signal_connect(G_OBJECT (about_item), "activate",
		   G_CALLBACK (on_about),
		   NULL);
  g_signal_connect(G_OBJECT (quit_item), "activate",
		   G_CALLBACK (gtk_main_quit),
		   NULL);


  /* We do need to show menu items */
  gtk_widget_show (toggle_paint_item);
  gtk_widget_show (clear_item);
  gtk_widget_show (toggle_vis_item);
  gtk_widget_show (thicker_lines_item);
  gtk_widget_show (thinner_lines_item);
  gtk_widget_show (opacity_bigger_item);
  gtk_widget_show (opacity_lesser_item);
  gtk_widget_show (undo_item);
  gtk_widget_show (redo_item);

  gtk_widget_show (sep1_item);
  gtk_widget_show (intro_item);
  gtk_widget_show (edit_config_item);
  gtk_widget_show (issues_item);
  gtk_widget_show (support_item);
  gtk_widget_show (about_item);

  gtk_widget_show (sep2_item);
  gtk_widget_show (quit_item);


  app_indicator_set_menu (data->trayicon, GTK_MENU(menu));

  /*
    Build the support menu
   */
  GtkWidget *support_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(support_item), support_menu);

  GtkWidget* support_liberapay_item = gtk_menu_item_new_with_label(_("Via LiberaPay"));
  GtkWidget* support_patreon_item = gtk_menu_item_new_with_label(_("Via Patreon"));
  GtkWidget* support_paypal_item = gtk_menu_item_new_with_label(_("Via PayPal"));

  gtk_menu_shell_append (GTK_MENU_SHELL (support_menu), support_liberapay_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (support_menu), support_patreon_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (support_menu), support_paypal_item);

  g_signal_connect(G_OBJECT (support_liberapay_item), "activate",
		   G_CALLBACK (on_support_liberapay),
		   data);
  g_signal_connect(G_OBJECT (support_patreon_item), "activate",
		   G_CALLBACK (on_support_patreon),
		   data);
  g_signal_connect(G_OBJECT (support_paypal_item), "activate",
		   G_CALLBACK (on_support_paypal),
		   data);

  gtk_widget_show(support_liberapay_item);
  gtk_widget_show(support_patreon_item);
  gtk_widget_show(support_paypal_item);


  if(data->show_intro_on_startup)
      on_intro(NULL, data);
}


void parse_print_help (gpointer key, gpointer value, gpointer user_data)
{
  paint_context_print ((gchar *) key, (GromitPaintContext *) value);
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
       else if (strcmp (arg, "-l") == 0 ||
           strcmp (arg, "--line") == 0)
         {
           if (argc - (i+1) == 6) /* this command must have exactly 6 params */
             {

                // check if provided values are valid coords on the screen
               if (atoi(argv[i+1]) < 0 || atoi(argv[i+1]) > (int)data->width ||
                   atoi(argv[i+2]) < 0 || atoi(argv[i+2]) > (int)data->height ||
                   atoi(argv[i+3]) < 0 || atoi(argv[i+3]) > (int)data->width ||
                   atoi(argv[i+4]) < 0 || atoi(argv[i+4]) > (int)data->height)
                    {
                      g_printerr ("Invalid coordinates\n");
                      wrong_arg = TRUE;
                    }
               else if (atoi(argv[i+6]) < 1)
                    {
                      g_printerr ("Thickness must be atleast 1\n");
                      wrong_arg = TRUE;
                    }
               else
                    {
                      data->clientdata = g_strjoin(" ", argv[i+1], argv[i+2], argv[i+3], argv[i+4], argv[i+5], argv[i+6], NULL);
                    }

               action = GA_LINE;
               i += 6;
             }
           else
             {
               g_printerr ("-l requires 6 parameters: startX, startY, endX, endY, color, thickness\n");
               wrong_arg = TRUE;
             }
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

  /*
      we run okay under XWayland, but not native Wayland
  */
  gdk_set_allowed_backends ("x11");

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
  data->opacity = DEFAULT_OPACITY;

  /*
    init our window
  */
  data->win = gtk_window_new (GTK_WINDOW_POPUP);
  // this trys to set an alpha channel
  on_screen_changed(data->win, NULL, data);

  gtk_window_fullscreen(GTK_WINDOW(data->win)); 
  gtk_window_set_skip_taskbar_hint(GTK_WINDOW(data->win), TRUE);
  gtk_widget_set_opacity(data->win, data->opacity);
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
  gtk_selection_add_target (data->win, GA_DATA, GA_LINEDATA, 1008);



  /* Try to get a status message. If there is a response gromit
   * is already active.
   */

  gtk_selection_convert (data->win, GA_CONTROL, GA_STATUS,
                         GDK_CURRENT_TIME);
  gtk_main ();  /* Wait for the response */

  if (data->client)
    return main_client (argc, argv, data);

  /* Main application */
  signal(SIGINT, on_signal);
  signal(SIGTERM, on_signal);
  setup_main_app (data, argc, argv);
  gtk_main ();
  shutdown_input_devices(data);
  write_keyfile(data); // save keyfile config
  g_free (data);
  return 0;
}

void indicate_active(GromitData *data, gboolean YESNO)
{
    if(YESNO)
	app_indicator_set_icon(data->trayicon, "net.christianbeier.Gromit-MPX.active");
    else
	app_indicator_set_icon(data->trayicon, "net.christianbeier.Gromit-MPX");
}
