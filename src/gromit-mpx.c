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

#include "gromit-mpx.h"
#include "callbacks.h"
#include "config.h"

#include "paint_cursor.xpm"
#include "erase_cursor.xpm"


/* Clear cairo context */
void clear_cairo_context(cairo_t* cr)
{
  if (cr)
    {
      cairo_save(cr);
      cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
      cairo_paint(cr);
      cairo_restore(cr);
    } 
}


GromitPaintContext *
gromit_paint_context_new (GromitData *data, GromitPaintType type,
                          GdkColor *fg_color, guint width, guint arrowsize)
{
  GromitPaintContext *context;

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
      context->paint_gc = gdk_cairo_create (data->pixmap);
      gdk_cairo_set_source_color(context->paint_gc, fg_color);
      cairo_set_line_width(context->paint_gc, width);
      cairo_set_line_cap(context->paint_gc, CAIRO_LINE_CAP_ROUND);
      cairo_set_line_join(context->paint_gc, CAIRO_LINE_JOIN_ROUND);
      // context->paint_gc = gdk_gc_new (data->pixmap);
      // gdk_gc_set_foreground (context->paint_gc, fg_color);
      //      gdk_gc_set_line_attributes (context->paint_gc, width, GDK_LINE_SOLID,
      //                            GDK_CAP_ROUND, GDK_JOIN_ROUND);
    }

  if (type == GROMIT_RECOLOR)
    {
      context->shape_gc = NULL;
    }
  else
    {
      /* GROMIT_PEN || GROMIT_ERASER */
      context->shape_gc = gdk_cairo_create (data->shape);
      // context->shape_gc = gdk_gc_new (data->shape);
      //      gdk_gc_get_values (context->shape_gc, &shape_gcv);

      if (type == GROMIT_ERASER)
	//FIXMEgdk_gc_set_foreground (context->shape_gc, &(shape_gcv.foreground));
	gdk_cairo_set_source_color(context->shape_gc, fg_color);
      else
         /* GROMIT_PEN */
	//FIXMEgdk_gc_set_foreground (context->shape_gc, &(shape_gcv.background));
	gdk_cairo_set_source_color(context->shape_gc, fg_color);

      //gdk_gc_set_line_attributes (context->shape_gc, width, GDK_LINE_SOLID,
      //                           GDK_CAP_ROUND, GDK_JOIN_ROUND);
      cairo_set_line_width(context->shape_gc, width);
      cairo_set_line_cap(context->shape_gc, CAIRO_LINE_CAP_ROUND);
      cairo_set_line_join(context->shape_gc, CAIRO_LINE_JOIN_ROUND);

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


void gromit_coord_list_prepend (GromitData *data, GdkDevice* dev, gint x, gint y, gint width)
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


void gromit_coord_list_free (GromitData *data, GdkDevice* dev)
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


gboolean gromit_coord_list_get_arrow_param (GromitData *data,
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
  gdk_window_raise (gtk_widget_get_window(data->win));
}



void gromit_toggle_visibility (GromitData *data)
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
          if(devdata->is_grabbed)
	  {
	    gdk_device_ungrab(devdata->device, GDK_CURRENT_TIME);
	    devdata->is_grabbed = 0;
	  }
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
      gdk_device_ungrab(devdata->device, GDK_CURRENT_TIME);
      devdata->is_grabbed = 0;

      if(data->debug)
        g_printerr ("DEBUG: Ungrabbed Device '%s'.\n", gdk_device_get_name(devdata->device));
    }

  if (!data->painted)
    gromit_hide_window (data);
}


void
gromit_acquire_grab (GromitData *data, GdkDevice *dev)
{
  gromit_show_window (data);

  if(!dev) /* this means grab all */
    {
      GHashTableIter it;
      gpointer value;
      GromitDeviceData* devdata = NULL; 
      g_hash_table_iter_init (&it, data->devdatatable);
      while (g_hash_table_iter_next (&it, NULL, &value)) 
        {
          GdkCursor *cursor;

	  devdata = value;
          if(devdata->is_grabbed)
            continue;

	  if(devdata->cur_context && devdata->cur_context->type == GROMIT_ERASER)
	    cursor = data->erase_cursor; 
	  else
	    cursor = data->paint_cursor; 

	
	  if(gdk_device_grab(devdata->device,
			     gtk_widget_get_window(data->area),
			     GDK_OWNERSHIP_NONE,
			     FALSE,
			     GROMIT_MOUSE_EVENTS,
			     cursor,
			     GDK_CURRENT_TIME) != GDK_GRAB_SUCCESS)
	    {
	      /* this probably means the device table is outdated, 
		 e.g. this device doesn't exist anymore */
	      g_printerr("Error grabbing Device '%s' while grabbing all, ignoring.\n", 
			 gdk_device_get_name(devdata->device));
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
      GdkCursor *cursor;
      if(devdata->cur_context && devdata->cur_context->type == GROMIT_ERASER)
	cursor = data->erase_cursor; 
      else
	cursor = data->paint_cursor; 
      
      if(gdk_device_grab(devdata->device,
			 gtk_widget_get_window(data->area),
			 GDK_OWNERSHIP_NONE,
			 FALSE,
			 GROMIT_MOUSE_EVENTS,
			 cursor,
			 GDK_CURRENT_TIME) != GDK_GRAB_SUCCESS)
	{
	  /* this probably means the device table is outdated,
	     e.g. this device doesn't exist anymore */
	  g_printerr("Error grabbing device '%s', rescanning device list.\n", 
		     gdk_device_get_name(devdata->device));
	  setup_input_devices(data);
	  return;
	}

      devdata->is_grabbed = 1;
      
      if(data->debug)
        g_printerr("DEBUG: Grabbed Device '%s'.\n", gdk_device_get_name(devdata->device));
    }

}


void gromit_toggle_grab (GromitData *data, GdkDevice* dev)
{
  if(dev == NULL) /* toggle all */
    {
      if (data->all_grabbed)
	gromit_release_grab (data, NULL);
      else
	gromit_acquire_grab (data, NULL);
      return; 
    }

  /* get the data for this device */
  GromitDeviceData *devdata = g_hash_table_lookup(data->devdatatable, dev);
      
  if(devdata)
    {
      if(devdata->is_grabbed)
        gromit_release_grab (data, devdata->device);
      else
        gromit_acquire_grab (data, devdata->device);
    }
  else
    g_printerr("ERROR: No such device '%s' in internal table.\n", gdk_device_get_name(dev));
}


void gromit_clear_screen (GromitData *data)
{
  //gdk_gc_set_foreground (data->shape_gc, data->transparent);
  //gdk_draw_rectangle (data->shape, data->shape_gc, 1,
  //                    0, 0, data->width, data->height);

  clear_cairo_context(data->shape_gc);

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


void gromit_select_tool (GromitData *data, GdkDevice *device, guint state)
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
      len = strlen (gdk_device_get_name(device));
      name = (guchar*) g_strndup (gdk_device_get_name(device), len + 3);
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
                  devdata->cur_context = context;
                  success = 1;
                }
              else /* try default_name */
                if((context = g_hash_table_lookup (data->tool_config, default_name)))
                  {
                    if(data->debug)
                      g_printerr("DEBUG: Default context %s set\n", default_name);
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
          if (gdk_device_get_source(device) == GDK_SOURCE_ERASER)
            devdata->cur_context = data->default_eraser;
          else
            devdata->cur_context = data->default_pen;
        }
    }
  else
    g_printerr ("ERROR: Attempt to select nonexistent device!\n");

  GdkCursor *cursor;
  if(devdata->cur_context && devdata->cur_context->type == GROMIT_ERASER)
    cursor = data->erase_cursor; 
  else
    cursor = data->paint_cursor; 
  gdk_window_set_device_cursor(gtk_widget_get_window(data->win), devdata->device, cursor);
   
  devdata->state = state;
}


void gromit_draw_line (GromitData *data, GdkDevice *dev, gint x1, gint y1,
		       gint x2, gint y2)
{
  GdkRectangle rect;
  GromitDeviceData *devdata = g_hash_table_lookup(data->devdatatable, dev);

  rect.x = MIN (x1,x2) - data->maxwidth / 2;
  rect.y = MIN (y1,y2) - data->maxwidth / 2;
  rect.width = ABS (x1-x2) + data->maxwidth;
  rect.height = ABS (y1-y2) + data->maxwidth;

  if (devdata->cur_context->paint_gc)
    {
      cairo_set_line_width(devdata->cur_context->paint_gc, data->maxwidth);
      cairo_set_line_cap(devdata->cur_context->paint_gc, CAIRO_LINE_CAP_ROUND);
      cairo_set_line_join(devdata->cur_context->paint_gc, CAIRO_LINE_JOIN_ROUND);

      //      gdk_gc_set_line_attributes (devdata->cur_context->paint_gc,
      //                          data->maxwidth, GDK_LINE_SOLID,
      //                          GDK_CAP_ROUND, GDK_JOIN_ROUND);
    }
  if (devdata->cur_context->shape_gc)
    {
      cairo_set_line_width(devdata->cur_context->shape_gc, data->maxwidth);
      cairo_set_line_cap(devdata->cur_context->shape_gc, CAIRO_LINE_CAP_ROUND);
      cairo_set_line_join(devdata->cur_context->shape_gc, CAIRO_LINE_JOIN_ROUND);

      //      gdk_gc_set_line_attributes (devdata->cur_context->shape_gc,
      //                          data->maxwidth, GDK_LINE_SOLID,
      //                          GDK_CAP_ROUND, GDK_JOIN_ROUND);
    }

  if (devdata->cur_context->paint_gc)
    {
      cairo_move_to(devdata->cur_context->paint_gc, x1, y1);
      cairo_line_to(devdata->cur_context->paint_gc, x2, y2);
      cairo_stroke(devdata->cur_context->paint_gc);

      //gdk_draw_line (data->pixmap, devdata->cur_context->paint_gc,
      //               x1, y1, x2, y2);
    }

  if (devdata->cur_context->shape_gc)
    {
      cairo_move_to(devdata->cur_context->shape_gc, x1, y1);
      cairo_line_to(devdata->cur_context->shape_gc, x2, y2);
      cairo_stroke(devdata->cur_context->shape_gc);

      //gdk_draw_line (data->shape, devdata->cur_context->shape_gc,
      //               x1, y1, x2, y2);
      data->modified = 1;
    }

  if (devdata->cur_context->paint_gc)
    gdk_window_invalidate_rect(gtk_widget_get_window(data->area), &rect, 0); 

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
    {
      cairo_set_line_width(devdata->cur_context->paint_gc, 0);
      cairo_set_line_cap(devdata->cur_context->paint_gc, CAIRO_LINE_CAP_ROUND);
      cairo_set_line_join(devdata->cur_context->paint_gc, CAIRO_LINE_JOIN_ROUND);

      // gdk_gc_set_line_attributes (devdata->cur_context->paint_gc,
      //                         0, GDK_LINE_SOLID,
      //                          GDK_CAP_ROUND, GDK_JOIN_ROUND);
    }

  if (devdata->cur_context->shape_gc)
    {
      cairo_set_line_width(devdata->cur_context->shape_gc, 0);
      cairo_set_line_cap(devdata->cur_context->shape_gc, CAIRO_LINE_CAP_ROUND);
      cairo_set_line_join(devdata->cur_context->shape_gc, CAIRO_LINE_JOIN_ROUND);

      //  gdk_gc_set_line_attributes (devdata->cur_context->shape_gc,
      //                          0, GDK_LINE_SOLID,
      //                         GDK_CAP_ROUND, GDK_JOIN_ROUND);
    }

  if (devdata->cur_context->paint_gc)
    {
      cairo_move_to(devdata->cur_context->paint_gc, arrowhead[0].x, arrowhead[0].y);
      cairo_line_to(devdata->cur_context->paint_gc, arrowhead[1].x, arrowhead[1].y);
      cairo_line_to(devdata->cur_context->paint_gc, arrowhead[2].x, arrowhead[2].y);
      cairo_line_to(devdata->cur_context->paint_gc, arrowhead[3].x, arrowhead[3].y);
      cairo_fill(devdata->cur_context->paint_gc);

      gdk_cairo_set_source_color(devdata->cur_context->paint_gc, data->black);

      cairo_move_to(devdata->cur_context->paint_gc, arrowhead[0].x, arrowhead[0].y);
      cairo_line_to(devdata->cur_context->paint_gc, arrowhead[1].x, arrowhead[1].y);
      cairo_line_to(devdata->cur_context->paint_gc, arrowhead[2].x, arrowhead[2].y);
      cairo_line_to(devdata->cur_context->paint_gc, arrowhead[3].x, arrowhead[3].y);
      cairo_stroke(devdata->cur_context->paint_gc);

      gdk_cairo_set_source_color(devdata->cur_context->paint_gc, devdata->cur_context->fg_color);
      

      /*gdk_draw_polygon (data->pixmap, devdata->cur_context->paint_gc,
                        TRUE, arrowhead, 4);
      gdk_gc_set_foreground (devdata->cur_context->paint_gc, data->black);
      gdk_draw_polygon (data->pixmap, devdata->cur_context->paint_gc,
                        FALSE, arrowhead, 4);
      gdk_gc_set_foreground (devdata->cur_context->paint_gc,
      devdata->cur_context->fg_color);*/
    }

  if (devdata->cur_context->shape_gc)
    {
      cairo_move_to(devdata->cur_context->shape_gc, arrowhead[0].x, arrowhead[0].y);
      cairo_line_to(devdata->cur_context->shape_gc, arrowhead[1].x, arrowhead[1].y);
      cairo_line_to(devdata->cur_context->shape_gc, arrowhead[2].x, arrowhead[2].y);
      cairo_line_to(devdata->cur_context->shape_gc, arrowhead[3].x, arrowhead[3].y);
      cairo_fill(devdata->cur_context->shape_gc);

      cairo_move_to(devdata->cur_context->shape_gc, arrowhead[0].x, arrowhead[0].y);
      cairo_line_to(devdata->cur_context->shape_gc, arrowhead[1].x, arrowhead[1].y);
      cairo_line_to(devdata->cur_context->shape_gc, arrowhead[2].x, arrowhead[2].y);
      cairo_line_to(devdata->cur_context->shape_gc, arrowhead[3].x, arrowhead[3].y);
      cairo_stroke(devdata->cur_context->shape_gc);

      /*      gdk_draw_polygon (data->shape, devdata->cur_context->shape_gc,
                        TRUE, arrowhead, 4);
      gdk_draw_polygon (data->shape, devdata->cur_context->shape_gc,
      FALSE, arrowhead, 4);*/
      data->modified = 1;
    }

  if (devdata->cur_context->paint_gc)
    gdk_window_invalidate_rect(gtk_widget_get_window(data->area), &rect, 0); 

  data->painted = 1;
}




/*
 * Functions for handling various (GTK+)-Events
 */


/* Keyboard control */

gint
key_press_event (GtkWidget   *grab_widget,
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
        gromit_clear_screen (data);
      else if (event->state & GDK_CONTROL_MASK)
        gromit_toggle_visibility (data);
      else if (event->state & GDK_MOD1_MASK)
        gtk_main_quit ();
      else
        gromit_toggle_grab (data, gdk_device_get_associated_device(dev));

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
      event->window = gtk_widget_get_window(data->win);
      g_object_ref (gtk_widget_get_window(data->win));
    }

  gtk_main_do_event ((GdkEvent *) event);
}




/*
 * Functions for setting up (parts of) the application
 */

void
setup_input_devices (GromitData *data)
{
  /* ungrab all */
  gromit_release_grab (data, NULL); 
  /* and clear our own device data list */
  g_hash_table_remove_all(data->devdatatable);


  /* get devices */
  GdkDeviceManager *device_manager = gdk_display_get_device_manager(data->display);
  GList *devices, *d;
  int i = 0;

  devices = gdk_device_manager_list_devices(device_manager, GDK_DEVICE_TYPE_MASTER);
  for(d = devices; d; d = d->next)
    {
      GdkDevice *device = (GdkDevice *) d->data;
    
      /* Guess "Eraser"-Type devices */
      if (strstr (gdk_device_get_name(device), "raser") || strstr (gdk_device_get_name(device), "RASER"))
	gdk_device_set_source (device, GDK_SOURCE_ERASER);

      /* only enable devices with 2 ore more axes */
      if (gdk_device_get_n_axes(device) >= 2)
        {
          gdk_device_set_mode (device, GDK_MODE_SCREEN);

          GromitDeviceData *devdata;

          devdata  = g_malloc0(sizeof (GromitDeviceData));
          devdata->device = device;
          devdata->index = i;

	  /* get attached keyboard and grab the hotkey */
	  if (!data->hot_keycode)
	      {
		g_printerr("ERROR: Grabbing hotkey from attached keyboard of '%s' failed, no hotkey defined.\n",
			   gdk_device_get_name(device));
		g_free(devdata);
		continue;
	      }

	  GdkDevice* attached_kbd = gdk_device_get_associated_device(device);
	  if(attached_kbd)
	    {
	      gint kbd_dev_id = -1;
	      g_object_get(attached_kbd, "device-id", &kbd_dev_id, NULL);
	      if(kbd_dev_id != -1)
		{
		  XIEventMask mask;
		  unsigned char bits[4] = {0,0,0,0};
		  mask.mask = bits;
		  mask.mask_len = sizeof(bits);
		  
		  XISetMask(bits, XI_KeyPress);
		  XISetMask(bits, XI_KeyRelease);

		  XIGrabModifiers modifiers[] = {{XIAnyModifier, 0}};
		  int nmods = 1;

		  gdk_error_trap_push ();

		  XIGrabKeycode( GDK_DISPLAY_XDISPLAY(data->display),
				 kbd_dev_id,
				 data->hot_keycode,
				 GDK_WINDOW_XWINDOW(data->root),
				 GrabModeAsync,
				 GrabModeAsync,
				 True,
				 &mask,
				 nmods,
				 modifiers);

		  gdk_flush ();
		  if(gdk_error_trap_pop())
		    {
		      g_printerr("ERROR: Grabbing hotkey from keyboard '%s' failed.\n",
				 gdk_device_get_name(attached_kbd));
		      g_free(devdata);
		      continue;
		    }
		}
	    }

         
	  g_hash_table_insert(data->devdatatable, device, devdata);
          g_printerr ("Enabled Device %d: \"%s\", (Type: %d)\n", 
		      i++, gdk_device_get_name(device), gdk_device_get_source(device));
        }
    }

  g_printerr ("Now %d enabled devices.\n", g_hash_table_size(data->devdatatable));
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

  gtk_window_fullscreen(GTK_WINDOW(data->win)); 
  gtk_window_set_skip_taskbar_hint(GTK_WINDOW(data->win), TRUE);
  gtk_window_set_opacity(GTK_WINDOW(data->win), 1); 

  gtk_widget_set_size_request (GTK_WIDGET (data->win), data->width, data->height);
  /* gtk_widget_set_uposition (GTK_WIDGET (data->win), 0, 0); */
  
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
  gboolean   have_key = FALSE;

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


  
  /* SHAPE PIXMAP */
  data->shape = gdk_pixmap_new (NULL, data->width, data->height, 1);
  data->shape_gc = gdk_cairo_create (data->shape);

  clear_cairo_context(data->shape_gc); 

  //data->shape_gcv = g_malloc (sizeof (GdkGCValues));
  //gdk_gc_get_values (data->shape_gc, data->shape_gcv);

  // FIXME!!
  //data->transparent = g_malloc(sizeof(GdkColor));
  //data->transparent = gdk_color_parse (NULL, data->transparent);
  //data->transparent = gdk_color_copy (&(data->shape_gcv->foreground));
  //data->opaque = gdk_color_copy (&(data->shape_gcv->background));

  /*data->transparent_pixmap = gdk_pixmap_new (data->shape,
					     data->width,
					     data->height,
					     -1);

  cairo_t* transparent_cr = gdk_cairo_create(data->transparent_pixmap);

  cairo_save(transparent_cr);
  cairo_set_operator (transparent_cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint(transparent_cr);
  cairo_restore(transparent_cr);

  gdk_cairo_set_source_pixmap (data->shape_gc,
			       data->transparent_pixmap,
			       0, 
			       0);
			       cairo_paint(data->shape_gc);*/


 
  //  gdk_gc_set_foreground (data->shape_gc, data->transparent);
  //gdk_draw_rectangle (data->shape, data->shape_gc,
  //                    1, 0, 0, data->width, data->height);




  /* DRAWING AREA */
  data->area = gtk_drawing_area_new ();
  gtk_widget_set_size_request(GTK_WIDGET(data->area),
			      data->width, data->height);

  
  /* EVENTS */
  gtk_widget_set_events (data->area, GROMIT_PAINT_AREA_EVENTS);
  g_signal_connect (data->area, "expose_event",
		    G_CALLBACK (event_expose), data);
  g_signal_connect (data->area,"configure_event",
		    G_CALLBACK (event_configure), data);
  g_signal_connect (data->screen,"monitors_changed",
		    G_CALLBACK (event_monitors_changed), data);
  g_signal_connect (gdk_display_get_device_manager (data->display), "device-added",
                    G_CALLBACK (device_added_cb), data);
  g_signal_connect (gdk_display_get_device_manager (data->display), "device-removed",
                    G_CALLBACK (device_removed_cb), data);
  g_signal_connect (data->win, "motion_notify_event",
		    G_CALLBACK (paintto), data);
  g_signal_connect (data->win, "button_press_event", 
		    G_CALLBACK (paint), data);
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



  gtk_container_add (GTK_CONTAINER (data->win), data->area);
  gtk_widget_shape_combine_mask (data->win, data->shape, 0,0);


  /*
   * Parse Config file
   */
  data->tool_config = g_hash_table_new (g_str_hash, g_str_equal);
  parse_config (data);
  if (data->debug)
    g_hash_table_foreach (data->tool_config, parse_print_help, NULL);
  

  /* FIND HOTKEY KEYCODE */
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


  /* INPUT DEVICES */
  data->devdatatable = g_hash_table_new(NULL, NULL);
  setup_input_devices (data);



  gtk_widget_show_all (data->area);

  data->painted = 0;
  gromit_hide_window (data);

  data->timeout_id = g_timeout_add (20, reshape, data);
 
  data->modified = 0;

  data->default_pen = gromit_paint_context_new (data, GROMIT_PEN,
                                                data->red, 7, 0);
  data->default_eraser = gromit_paint_context_new (data, GROMIT_ERASER,
                                                   data->red, 75, 0);

  
 
  
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

  /* make sure we get really get MPX */
  gdk_enable_multidevice();

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

