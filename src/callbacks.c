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
#include <stdint.h>
#include <math.h>
#include "main.h"
#include "input.h"
#include "callbacks.h"
#include "config.h"
#include "drawing.h"
#include "build-config.h"
#include "coordlist_ops.h"

gboolean on_expose (GtkWidget *widget,
		    cairo_t* cr,
		    gpointer user_data)
{
  GromitData *data = (GromitData *) user_data;

  if(data->debug)
    g_printerr("DEBUG: got draw event\n");

  cairo_save (cr);
  cairo_set_source_surface (cr, data->backbuffer, 0, 0);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint (cr);
  cairo_restore (cr);

  if (data->debug) {
      // draw a pink background to know where the window is
      cairo_save (cr);
      cairo_set_source_rgba(cr, 64, 0, 64, 32);
      cairo_set_operator (cr, CAIRO_OPERATOR_DEST_OVER);
      cairo_paint (cr);
      cairo_restore (cr);
  }

  return TRUE;
}




gboolean on_configure (GtkWidget *widget,
		       GdkEventExpose *event,
		       gpointer user_data)
{
  GromitData *data = (GromitData *) user_data;

  if(data->debug)
    g_printerr("DEBUG: got configure event\n");

  return TRUE;
}



void on_screen_changed(GtkWidget *widget,
		       GdkScreen *previous_screen,
		       gpointer   user_data)
{
  GromitData *data = (GromitData *) user_data;

  if(data->debug)
    g_printerr("DEBUG: got screen-changed event\n");

  GdkScreen *screen = gtk_widget_get_screen(GTK_WIDGET (widget));
  GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
  if (visual == NULL)
    visual = gdk_screen_get_system_visual (screen);

  gtk_widget_set_visual (widget, visual);
}



void on_monitors_changed ( GdkScreen *screen,
			   gpointer   user_data) 
{
  GromitData *data = (GromitData *) user_data;

  // get new sizes
  data->width = gdk_screen_get_width (data->screen);
  data->height = gdk_screen_get_height (data->screen);

  if(data->debug)
    g_printerr("DEBUG: screen size changed to %d x %d!\n", data->width, data->height);

  // change size
  gtk_widget_set_size_request(GTK_WIDGET(data->win), data->width, data->height);
  // try to set transparent for input
  cairo_region_t* r =  cairo_region_create();
  gtk_widget_input_shape_combine_region(data->win, r);
  cairo_region_destroy(r);

  /* recreate the shape surface */
  cairo_surface_t *new_shape = cairo_image_surface_create(CAIRO_FORMAT_ARGB32 ,data->width, data->height);
  cairo_t *cr = cairo_create (new_shape);
  cairo_set_source_surface (cr, data->backbuffer, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);
  cairo_surface_destroy(data->backbuffer);
  data->backbuffer = new_shape;

  // recreate auxiliary backbuffer
  cairo_surface_destroy(data->aux_backbuffer);
  data->aux_backbuffer = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, data->width, data->height);

  /*
     these depend on the shape surface
  */
  GHashTableIter it;
  gpointer value;
  g_hash_table_iter_init (&it, data->tool_config);
  while (g_hash_table_iter_next (&it, NULL, &value)) 
    paint_context_free(value);
  g_hash_table_remove_all(data->tool_config);


  parse_config(data); // also calls paint_context_new() :-(


  data->default_pen = paint_context_new (data, GROMIT_PEN, data->red, 7, 0, GROMIT_ARROW_END,
                                         5, 10, 15, 25, 1, 0, G_MAXUINT);
  data->default_eraser = paint_context_new (data, GROMIT_ERASER, data->red, 75, 0, GROMIT_ARROW_END,
                                            5, 10, 15, 25, 1, 0, G_MAXUINT);

  if(!data->composited) // set shape
    {
      cairo_region_t* r = gdk_cairo_region_create_from_surface(data->backbuffer);
      gtk_widget_shape_combine_region(data->win, r);
      cairo_region_destroy(r);
    }

  setup_input_devices(data);


  gtk_widget_show_all (data->win);
}



void on_composited_changed ( GdkScreen *screen,
			   gpointer   user_data)
{
  GromitData *data = (GromitData *) user_data;

  if(data->debug)
    g_printerr("DEBUG: got composited-changed event\n");

  data->composited = gdk_screen_is_composited (data->screen);

  if(data->composited)
    {
      // undo shape
      gtk_widget_shape_combine_region(data->win, NULL);
      // re-apply transparency
      gtk_widget_set_opacity(data->win, 0.75);
    }

  // set anti-aliasing
  GHashTableIter it;
  gpointer value;
  g_hash_table_iter_init (&it, data->tool_config);
  while (g_hash_table_iter_next (&it, NULL, &value)) 
    {
      GromitPaintContext *context = value;
      cairo_set_antialias(context->paint_ctx, data->composited ? CAIRO_ANTIALIAS_DEFAULT : CAIRO_ANTIALIAS_NONE);
    }
      

  GdkRectangle rect = {0, 0, data->width, data->height};
  gdk_window_invalidate_rect(gtk_widget_get_window(data->win), &rect, 0); 
}



void on_clientapp_selection_get (GtkWidget          *widget,
				 GtkSelectionData   *selection_data,
				 guint               info,
				 guint               time,
				 gpointer            user_data)
{
  GromitData *data = (GromitData *) user_data;
  
  gchar *ans = "";

  if(data->debug)
    g_printerr("DEBUG: clientapp received request.\n");  


  if (gtk_selection_data_get_target(selection_data) == GA_TOGGLEDATA || gtk_selection_data_get_target(selection_data) == GA_LINEDATA)
    {
      ans = data->clientdata;
    }
    
  gtk_selection_data_set (selection_data,
                          gtk_selection_data_get_target(selection_data),
                          8, (guchar*)ans, strlen (ans));
}


void on_clientapp_selection_received (GtkWidget *widget,
				      GtkSelectionData *selection_data,
				      guint time,
				      gpointer user_data)
{
  GromitData *data = (GromitData *) user_data;

  /* If someone has a selection for us, Gromit is already running. */

  if(gtk_selection_data_get_data_type(selection_data) == GDK_NONE)
    data->client = 0;
  else
    data->client = 1;

  gtk_main_quit ();
}



static float line_thickener = 0;


gboolean on_buttonpress (GtkWidget *win, 
			 GdkEventButton *ev,
			 gpointer user_data)
{
  GromitData *data = (GromitData *) user_data;
  gdouble pressure = 1;

  /* get the data for this device */
  GromitDeviceData *devdata = g_hash_table_lookup(data->devdatatable, ev->device);

  if(data->debug)
    g_printerr("DEBUG: Device '%s': Button %i Down State %d at (x,y)=(%.2f : %.2f)\n",
	       gdk_device_get_name(ev->device), ev->button, ev->state, ev->x, ev->y);

  if (!devdata->is_grabbed)
    return FALSE;

  if (gdk_device_get_source(gdk_event_get_source_device((GdkEvent *)ev)) == GDK_SOURCE_PEN) {
      /* Do not drop unprocessed motion events. Smoother drawing for pens of tablets. */
      gdk_window_set_event_compression(gtk_widget_get_window(data->win), FALSE);
  } else {
      /* For all other source types, set back to default. Otherwise, lines were only
	 fully drawn to the end on button release. */
      gdk_window_set_event_compression(gtk_widget_get_window(data->win), TRUE);
  }

  /* See GdkModifierType. Am I fixing a Gtk misbehaviour???  */
  ev->state |= 1 << (ev->button + 7);


  if (ev->state != devdata->state ||
      devdata->lastslave != gdk_event_get_source_device ((GdkEvent *) ev))
    select_tool (data, ev->device, gdk_event_get_source_device ((GdkEvent *) ev), ev->state);

  GromitPaintType type = devdata->cur_context->type;

  // store original state to have dynamic update of line and rect
  if (type == GROMIT_LINE || type == GROMIT_RECT || type == GROMIT_SMOOTH || type == GROMIT_ORTHOGONAL || type == GROMIT_CIRCLE)
    {
      copy_surface(data->aux_backbuffer, data->backbuffer);
    }

  devdata->lastx = ev->x;
  devdata->lasty = ev->y;
  devdata->motion_time = ev->time;

  snap_undo_state (data);

  gdk_event_get_axis ((GdkEvent *) ev, GDK_AXIS_PRESSURE, &pressure);
  data->maxwidth = (CLAMP (pressure + line_thickener, 0, 1) *
		    (double) (devdata->cur_context->width -
			      devdata->cur_context->minwidth) +
		    devdata->cur_context->minwidth);

  if(data->maxwidth > devdata->cur_context->maxwidth)
    data->maxwidth = devdata->cur_context->maxwidth;

  if (ev->button <= 5)
    draw_line (data, ev->device, ev->x, ev->y, ev->x, ev->y);

  coord_list_prepend (data, ev->device, ev->x, ev->y, data->maxwidth);

  return TRUE;
}


gboolean on_motion (GtkWidget *win,
		    GdkEventMotion *ev,
		    gpointer user_data)
{
  GromitData *data = (GromitData *) user_data;
  GdkTimeCoord **coords = NULL;
  gint nevents;
  int i;
  gdouble pressure = 1;
  /* get the data for this device */
  GromitDeviceData *devdata = g_hash_table_lookup(data->devdatatable, ev->device);

  if (!devdata->is_grabbed)
    return FALSE;

  if(data->debug)
      g_printerr("DEBUG: Device '%s': motion to (x,y)=(%.2f : %.2f)\n", gdk_device_get_name(ev->device), ev->x, ev->y);

  if (ev->state != devdata->state ||
      devdata->lastslave != gdk_event_get_source_device ((GdkEvent *) ev))
    select_tool (data, ev->device, gdk_event_get_source_device ((GdkEvent *) ev), ev->state);

  GromitPaintType type = devdata->cur_context->type;

  gdk_device_get_history (ev->device, ev->window,
			  devdata->motion_time, ev->time,
			  &coords, &nevents);

  if(!data->xinerama && nevents > 0)
    {
      if (type != GROMIT_LINE && type != GROMIT_RECT)
        {
          for (i=0; i < nevents; i++)
            {
              gdouble x, y;

              gdk_device_get_axis (ev->device, coords[i]->axes,
                                   GDK_AXIS_PRESSURE, &pressure);
              if (pressure > 0)
                {
                  data->maxwidth = (CLAMP (pressure + line_thickener, 0, 1) *
                                    (double) (devdata->cur_context->width -
                                              devdata->cur_context->minwidth) +
                                    devdata->cur_context->minwidth);

                  if(data->maxwidth > devdata->cur_context->maxwidth)
                    data->maxwidth = devdata->cur_context->maxwidth;

                  gdk_device_get_axis(ev->device, coords[i]->axes,
                                      GDK_AXIS_X, &x);
                  gdk_device_get_axis(ev->device, coords[i]->axes,
                                      GDK_AXIS_Y, &y);

                  draw_line (data, ev->device, devdata->lastx, devdata->lasty, x, y);

                  coord_list_prepend (data, ev->device, x, y, data->maxwidth);
                  devdata->lastx = x;
                  devdata->lasty = y;
                }
            }
        }

      devdata->motion_time = coords[nevents-1]->time;
      g_free (coords);
    }

  /* always paint to the current event coordinate. */
  gdk_event_get_axis ((GdkEvent *) ev, GDK_AXIS_PRESSURE, &pressure);

  if (pressure > 0)
    {
      data->maxwidth = (CLAMP (pressure + line_thickener, 0, 1) *
			(double) (devdata->cur_context->width -
				  devdata->cur_context->minwidth) +
			devdata->cur_context->minwidth);

      if(data->maxwidth > devdata->cur_context->maxwidth)
	data->maxwidth = devdata->cur_context->maxwidth;

      if(devdata->motion_time > 0)
	{
          if (type == GROMIT_LINE || type == GROMIT_RECT || type == GROMIT_CIRCLE) {
            copy_surface(data->backbuffer, data->aux_backbuffer);
            GdkRectangle rect = {0, 0, data->width, data->height};
            gdk_window_invalidate_rect(gtk_widget_get_window(data->win), &rect, 0);
          }
          if (type == GROMIT_LINE)
            {
              GromitArrowType atype = devdata->cur_context->arrow_type;
	      draw_line (data, ev->device, devdata->lastx, devdata->lasty, ev->x, ev->y);
              if (devdata->cur_context->arrowsize > 0)
                {
                  GromitArrowType atype = devdata->cur_context->arrow_type;
                  gfloat width = devdata->cur_context->arrowsize * devdata->cur_context->width;
                  gfloat direction =
                      atan2(ev->y - devdata->lasty, ev->x - devdata->lastx);
                  if (atype & GROMIT_ARROW_END)
                    draw_arrow(data, ev->device, ev->x, ev->y, width, direction);
                  if (atype & GROMIT_ARROW_START)
                    draw_arrow(data, ev->device, devdata->lastx, devdata->lasty, width, M_PI + direction);
                }
            }
          else if (type == GROMIT_RECT)
            {
              draw_line (data, ev->device, devdata->lastx, devdata->lasty, ev->x, devdata->lasty);
              draw_line (data, ev->device, ev->x, devdata->lasty, ev->x, ev->y);
              draw_line (data, ev->device, ev->x, ev->y, devdata->lastx, ev->y);
              draw_line (data, ev->device, devdata->lastx, ev->y, devdata->lastx, devdata->lasty);
            }
          else if (type == GROMIT_CIRCLE)
            {
              float radius = sqrt(pow(ev->x - devdata->lastx, 2) + pow(ev->y - devdata->lasty, 2));
              draw_circle (data, ev->device, devdata->lastx, devdata->lasty, radius);
            }
          else
            {
              draw_line (data, ev->device, devdata->lastx, devdata->lasty, ev->x, ev->y);
	      coord_list_prepend (data, ev->device, ev->x, ev->y, data->maxwidth);
            }
	}
    }

  if (type != GROMIT_LINE && type != GROMIT_RECT && type != GROMIT_CIRCLE)
    {
      devdata->lastx = ev->x;
      devdata->lasty = ev->y;
    }
  devdata->motion_time = ev->time;

  return TRUE;
}


gboolean on_buttonrelease (GtkWidget *win, 
			   GdkEventButton *ev, 
			   gpointer user_data)
{
  GromitData *data = (GromitData *) user_data;
  /* get the device data for this event */
  GromitDeviceData *devdata = g_hash_table_lookup(data->devdatatable, ev->device);
  GromitPaintContext *ctx = devdata->cur_context;

  gfloat direction = 0;
  gint width = 0;
  if(ctx)
    width = ctx->arrowsize * ctx->width / 2;

  if ((ev->x != devdata->lastx) ||
      (ev->y != devdata->lasty))
    on_motion(win, (GdkEventMotion *) ev, user_data);

  if (!devdata->is_grabbed)
    return FALSE;

  GromitPaintType type = ctx->type;

  if (type == GROMIT_SMOOTH || type == GROMIT_ORTHOGONAL)
    {
      gboolean joined = FALSE;
      douglas_peucker(devdata->coordlist, ctx->simplify);
      if (ctx->snapdist > 0)
        joined = snap_ends(devdata->coordlist, ctx->snapdist);
      if (type == GROMIT_SMOOTH) {
          add_points(devdata->coordlist, 200);
          devdata->coordlist = catmull_rom(devdata->coordlist, 5, joined);
      } else {
          orthogonalize(devdata->coordlist, ctx->maxangle, ctx->minlen);
          round_corners(devdata->coordlist, ctx->radius, 6, joined);
      }

      copy_surface(data->backbuffer, data->aux_backbuffer);
      GdkRectangle rect = {0, 0, data->width, data->height};
      gdk_window_invalidate_rect(gtk_widget_get_window(data->win), &rect, 0);

      GList *ptr = devdata->coordlist;
      while (ptr && ptr->next)
        {
          GromitStrokeCoordinate *c1 = ptr->data;
          GromitStrokeCoordinate *c2 = ptr->next->data;
          ptr = ptr->next;
          draw_line (data, ev->device, c1->x, c1->y, c2->x, c2->y);
        }
    }
  else if (type == GROMIT_CIRCLE)
    {
      float radius = sqrt(pow(ev->x - devdata->lastx, 2) + pow(ev->y - devdata->lasty, 2));

      copy_surface(data->backbuffer, data->aux_backbuffer);
      GdkRectangle rect = {0, 0, data->width, data->height};
      gdk_window_invalidate_rect(gtk_widget_get_window(data->win), &rect, 0);

      draw_circle (data, ev->device, devdata->lastx, devdata->lasty, radius);
    }

  if (ctx->arrowsize != 0)
    {
      GromitArrowType atype = ctx->arrow_type;
      if (type == GROMIT_LINE)
        {
          direction = atan2 (ev->y - devdata->lasty, ev->x - devdata->lastx);
          if (atype & GROMIT_ARROW_END)
            draw_arrow(data, ev->device, ev->x, ev->y, width * 2, direction);
          if (atype & GROMIT_ARROW_START)
            draw_arrow(data, ev->device, devdata->lastx, devdata->lasty, width * 2, M_PI + direction);
        }
      else
        {
          gint x0, y0;
          if ((atype & GROMIT_ARROW_END) &&
              coord_list_get_arrow_param (data, ev->device, width * 3,
                                          GROMIT_ARROW_END, &x0, &y0, &width, &direction))
            draw_arrow (data, ev->device, x0, y0, width, direction);
          if ((atype & GROMIT_ARROW_START) &&
              coord_list_get_arrow_param (data, ev->device, width * 3,
                                          GROMIT_ARROW_START, &x0, &y0, &width, &direction)) {
            draw_arrow (data, ev->device, x0, y0, width, direction);
          }
        }
    }

  coord_list_free (data, ev->device);

  return TRUE;
}

/* Remote control */
void on_mainapp_selection_get (GtkWidget          *widget,
			       GtkSelectionData   *selection_data,
			       guint               info,
			       guint               time,
			       gpointer            user_data)
{
  GromitData *data = (GromitData *) user_data;
  
  gchar *uri = "OK";
  GdkAtom action = gtk_selection_data_get_target(selection_data);

  if(action == GA_TOGGLE)
    {
      /* ask back client for device id */
      gtk_selection_convert (data->win, GA_DATA,
                             GA_TOGGLEDATA, time);
      gtk_main(); /* Wait for the response */
    }
  else if(action == GA_LINE)
    {
      /* ask back client for device id */
      gtk_selection_convert (data->win, GA_DATA,
                             GA_LINEDATA, time);
      gtk_main(); /* Wait for the response */
    }
  else if (action == GA_VISIBILITY)
    toggle_visibility (data);
  else if (action == GA_CLEAR)
    clear_screen (data);
  else if (action == GA_RELOAD)
    setup_input_devices(data);
  else if (action == GA_QUIT)
    gtk_main_quit ();
  else if (action == GA_UNDO)
    undo_drawing (data);
  else if (action == GA_REDO)
    redo_drawing (data);
  else
    uri = "NOK";

   
  gtk_selection_data_set (selection_data,
                          gtk_selection_data_get_target(selection_data),
                          8, (guchar*)uri, strlen (uri));
}


void on_mainapp_selection_received (GtkWidget *widget,
				    GtkSelectionData *selection_data,
				    guint time,
				    gpointer user_data)
{
  GromitData *data = (GromitData *) user_data;

  if(gtk_selection_data_get_length(selection_data) < 0)
    {
      if(data->debug)
        g_printerr("DEBUG: mainapp got no answer back from client.\n");
    }
  else
    {
      if(gtk_selection_data_get_target(selection_data) == GA_TOGGLEDATA )
        {
	  intptr_t dev_nr = strtoull((gchar*)gtk_selection_data_get_data(selection_data), NULL, 10);
	  
          if(data->debug)
	    g_printerr("DEBUG: mainapp got toggle id '%ld' back from client.\n", (long)dev_nr);

	  if(dev_nr < 0)
	    toggle_grab(data, NULL); /* toggle all */
	  else 
	    {
	      /* find dev numbered dev_nr */
	      GHashTableIter it;
	      gpointer value;
	      GromitDeviceData* devdata = NULL; 
	      g_hash_table_iter_init (&it, data->devdatatable);
	      while (g_hash_table_iter_next (&it, NULL, &value)) 
		{
		  devdata = value;
		  if(devdata->index == dev_nr)
		    break;
		  else
		    devdata = NULL;
		}
	      
	      if(devdata)
		toggle_grab(data, devdata->device);
	      else
		g_printerr("ERROR: No device at index %ld.\n", (long)dev_nr);
	    }
        }
      else if (gtk_selection_data_get_target(selection_data) == GA_LINEDATA) 
	{

	  gchar** line_args = g_strsplit((gchar*)gtk_selection_data_get_data(selection_data), " ", 6);
	  int startX = atoi(line_args[0]);
	  int startY = atoi(line_args[1]);
	  int endX = atoi(line_args[2]);
	  int endY = atoi(line_args[3]);
	  gchar* hex_code = line_args[4];
	  int thickness = atoi(line_args[5]);

          if(data->debug) 
	    {
	      g_printerr("DEBUG: mainapp got line data back from client:\n");
	      g_printerr("startX startY endX endY: %d %d %d %d\n", startX, startY, endX, endY);
	      g_printerr("color: %s\n", hex_code);
	      g_printerr("thickness: %d\n", thickness);
	    }

	  GdkRGBA* color = g_malloc (sizeof (GdkRGBA));
	  GdkRGBA *fg_color=data->red;
	  if (gdk_rgba_parse (color, hex_code))
	    {
	      fg_color = color;
	    }
	  else
	    {
	      g_printerr ("Unable to parse color. "
	      "Keeping default.\n");
	    }
	  GromitPaintContext* line_ctx =
            paint_context_new(data, GROMIT_PEN, fg_color, thickness, 0, GROMIT_ARROW_END,
                              5, 10, 15, 25, 0, thickness, thickness);

	  GdkRectangle rect;
	  rect.x = MIN (startX,endX) - thickness / 2;
	  rect.y = MIN (startY,endY) - thickness / 2;
	  rect.width = ABS (startX-endX) + thickness;
	  rect.height = ABS (startY-endY) + thickness;

	  if(data->debug)
	    g_printerr("DEBUG: draw line from %d %d to %d %d\n", startX, startY, endX, endY);

	  cairo_set_line_width(line_ctx->paint_ctx, thickness);
	  cairo_move_to(line_ctx->paint_ctx, startX, startY);
	  cairo_line_to(line_ctx->paint_ctx, endX, endY);
	  cairo_stroke(line_ctx->paint_ctx);

	  data->modified = 1;
	  gdk_window_invalidate_rect(gtk_widget_get_window(data->win), &rect, 0); 
	  data->painted = 1;

	  g_free(line_ctx);
	  g_free (color);
	}
    }
 
  gtk_main_quit ();
}


void on_device_removed (GdkDeviceManager *device_manager,
			GdkDevice        *device,
			gpointer          user_data)
{
  GromitData *data = (GromitData *) user_data;
    
  if(gdk_device_get_device_type(device) != GDK_DEVICE_TYPE_MASTER
     || gdk_device_get_n_axes(device) < 2)
    return;
  
  if(data->debug)
    g_printerr("DEBUG: device '%s' removed\n", gdk_device_get_name(device));

  setup_input_devices(data);
}

void on_device_added (GdkDeviceManager *device_manager,
		      GdkDevice        *device,
		      gpointer          user_data)
{
  GromitData *data = (GromitData *) user_data;

  if(gdk_device_get_device_type(device) != GDK_DEVICE_TYPE_MASTER
     || gdk_device_get_n_axes(device) < 2)
    return;

  if(data->debug)
    g_printerr("DEBUG: device '%s' added\n", gdk_device_get_name(device));

  setup_input_devices(data);
}



gboolean on_toggle_paint(GtkWidget *widget,
			 GdkEventButton  *ev,
			 gpointer   user_data)
{
    GromitData *data = (GromitData *) user_data;

    if(data->debug)
	g_printerr("DEBUG: Device '%s': Button %i on_toggle_paint at (x,y)=(%.2f : %.2f)\n",
		   gdk_device_get_name(ev->device), ev->button, ev->x, ev->y);

    toggle_grab(data, ev->device);

    return TRUE;
}

void on_toggle_paint_all (GtkMenuItem *menuitem,
			  gpointer     user_data)
{
  GromitData *data = (GromitData *) user_data;

  /*
    on_toggle_paint_all() is called when toggle_paint_item in the menu
    is clicked on KDE-like platforms. Under X11, at least
    https://github.com/ubuntu/gnome-shell-extension-appindicator seems to
    grab the pointer, preventing grabbing by Gromit-MPX.
    Simply work around this by waiting :-/
   */
  char *xdg_session_type = getenv("XDG_SESSION_TYPE");
  if (xdg_session_type && strcmp(xdg_session_type, "x11") == 0)
      g_usleep(333*1000);

  toggle_grab(data, NULL);
}


void on_clear (GtkMenuItem *menuitem,
	       gpointer     user_data)
{
  GromitData *data = (GromitData *) user_data;
  clear_screen(data);
}


void on_toggle_vis(GtkMenuItem *menuitem,
		   gpointer     user_data)
{
  GromitData *data = (GromitData *) user_data;
  toggle_visibility(data);
}


void on_thicker_lines(GtkMenuItem *menuitem,
		      gpointer     user_data)
{
  line_thickener += 0.1;
}

void on_thinner_lines(GtkMenuItem *menuitem,
		      gpointer     user_data)
{
  line_thickener -= 0.1;
  if (line_thickener < -1)
    line_thickener = -1;
}


void on_opacity_bigger(GtkMenuItem *menuitem,
		       gpointer     user_data)
{
  GromitData *data = (GromitData *) user_data;
  data->opacity += 0.1;
  if(data->opacity>1.0)
    data->opacity = 1.0;
  gtk_widget_set_opacity(data->win, data->opacity);
}

void on_opacity_lesser(GtkMenuItem *menuitem,
		       gpointer     user_data)
{
  GromitData *data = (GromitData *) user_data;
  data->opacity -= 0.1;
  if(data->opacity<0.0)
    data->opacity = 0.0;
  gtk_widget_set_opacity(data->win, data->opacity);
}


void on_undo(GtkMenuItem *menuitem,
	     gpointer     user_data)
{
  GromitData *data = (GromitData *) user_data;
  undo_drawing (data);
}

void on_redo(GtkMenuItem *menuitem,
	     gpointer     user_data)
{
  GromitData *data = (GromitData *) user_data;
  redo_drawing (data);
}


void on_about(GtkMenuItem *menuitem,
	      gpointer     user_data)
{
    const gchar *authors [] = { "Christian Beier <info@christianbeier.net>",
                                "Simon Budig <Simon.Budig@unix-ag.org>",
                                "Barak A. Pearlmutter <barak+git@pearlmutter.net>",
                                "Nathan Whitehead <nwhitehe@gmail.com>",
                                "Lukáš Hermann <tuxilero@gmail.com>",
                                "Katie Holly <git@meo.ws>",
                                "Monty Montgomery <xiphmont@gmail.com>",
                                "AlisterH <alister.hood@gmail.com>",
                                "Mehmet Atif Ergun <mehmetaergun@users.noreply.github.com>",
                                "Russel Winder <russel@winder.org.uk>",
                                "Tao Klerks <tao@klerks.biz>",
                                "Tobias Schönberg <tobias47n9e@gmail.com>",
                                "Yuri D'Elia <yuri.delia@eurac.edu>",
				"Julián Unrrein <junrrein@gmail.com>",
				"Eshant Gupta <guptaeshant@gmail.com>",
				"marput <frayedultrasonicaligator@disroot.org>",
				"albanobattistella <34811668+albanobattistella@users.noreply.github.com>",
				"Renato Candido <renatocan@gmail.com>",
				"Komeil Parseh <ahmdparsh129@gmail.com>",
				"Adam Chyła <adam@chyla.org>",
				"bbhtt <62639087+bbhtt@users.noreply.github.com>",
				"avma <avi.markovitz@gmail.com>",
				"godmar <godmar@gmail.com>",
				"Ashwin Rajesh <46510831+VanillaViking@users.noreply.github.com>",
                                "Pascal Niklaus <pascal.niklaus@ieu.uzh.ch>",
                                "Renato Lima <natenho@gmail.com>",
                                "yzemaze <dev@yzemaze.de>",
                                "woodhead2019 <52210283+woodhead2019@users.noreply.github.com>",
                                "Leonardo Taccari <leot@NetBSD.org>",
                                "Andrew Marshall <andrew@johnandrewmarshall.com>",
                                "gavine99 <63904295+gavine99@users.noreply.github.com>",
                                "Marcin Orlowski <mail@marcinOrlowski.com>",
				NULL };
    gtk_show_about_dialog (NULL,
			   "program-name", "Gromit-MPX",
			   "logo-icon-name", "net.christianbeier.Gromit-MPX",
			   "title", _("About Gromit-MPX"),
			   "comments", _("Gromit-MPX (GRaphics Over MIscellaneous Things - Multi-Pointer-EXtension) is an on-screen annotation tool that works with any Unix desktop environment under X11 as well as Wayland."),
			   "version", PACKAGE_VERSION,
			   "website", PACKAGE_URL,
			   "authors", authors,
			   "copyright", "2009-2025 Christian Beier, Copyright 2000 Simon Budig",
			   "license-type", GTK_LICENSE_GPL_2_0,
			   NULL);
}


static void on_intro_show_again_button_toggled(GtkCheckButton *toggle, GromitData *data)
{
  data->show_intro_on_startup = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle));
}

void on_intro(GtkMenuItem *menuitem,
	      gpointer user_data)
{
    GromitData *data = (GromitData *) user_data;

    // Create a new assistant widget with no pages.
    GtkWidget *assistant = gtk_assistant_new ();
    gtk_window_set_position (GTK_WINDOW(assistant), GTK_WIN_POS_CENTER);

    // set page one
    GtkWidget *widgetOne = gtk_label_new(_("Gromit-MPX (GRaphics Over MIscellaneous Things) is a small tool to make\n"
					  "annotations on the screen.\n\n"
					  "Its main use is for making presentations of some application. Normally,\n"
					  "you would have to move the mouse pointer around the point of interest\n"
					  "until hopefully everybody noticed it.  With Gromit-MPX, you can draw\n"
					  "everywhere onto the screen, highlighting some button or area.\n\n"
                                          "If you happen to enjoy using Gromit-MPX, please consider supporting\n"
					  "its development by using one of the donation options on the project's\n"
					  "website or directly via the support options available from the tray menu.\n"));
    gtk_assistant_append_page (GTK_ASSISTANT (assistant), widgetOne);
    gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), widgetOne, _("Gromit-MPX - What is it?"));
    gtk_assistant_set_page_type (GTK_ASSISTANT (assistant), widgetOne, GTK_ASSISTANT_PAGE_INTRO);
    gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), widgetOne, TRUE);

    // set page two
    GtkWidget *widgetTwo = gtk_label_new (NULL);
    char widgetTwoBuf[4096];
    snprintf(widgetTwoBuf, sizeof(widgetTwoBuf),
	     _("You can operate Gromit-MPX using its tray icon (if your desktop environment\n"
	     "provides a sys tray), but since you typically want to use the program you are\n"
	     "demonstrating and highlighting something is a short interruption of your\n"
	     "workflow, Gromit-MPX can be toggled on and off on the fly via a hotkey:\n\n"
	     "It grabs the `%s` and `%s` keys, so that no other application can use them\n"
	     "and they are available to Gromit-MPX only.  The available commands are:\n\n<tt><b>"
	     "   toggle painting:         %s\n"
	     "   clear screen:            SHIFT-%s\n"
	     "   toggle visibility:       CTRL-%s\n"
	     "   quit:                    ALT-%s\n"
	     "   undo last stroke:        %s\n"
	     "   redo last undone stroke: SHIFT-%s</b></tt>"),
	     data->hot_keyval, data->undo_keyval,
	     data->hot_keyval, data->hot_keyval, data->hot_keyval, data->hot_keyval, data->undo_keyval, data->undo_keyval);
    gtk_label_set_markup (GTK_LABEL (widgetTwo), widgetTwoBuf);
    gtk_assistant_append_page (GTK_ASSISTANT (assistant), widgetTwo);
    gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), widgetTwo, _("Gromit-MPX - How to use it"));
    gtk_assistant_set_page_type (GTK_ASSISTANT (assistant), widgetTwo, GTK_ASSISTANT_PAGE_CONTENT);
    gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), widgetTwo, TRUE);

    // set page three
    GtkWidget *widgetThree = gtk_grid_new ();
    GtkWidget *widgetThreeText = gtk_label_new (_("Do you want to show this introduction again on the next start of Gromit-MPX?\n"
						  "You can always access it again via the sys tray menu.\n"));
    GtkWidget *widgetThreeButton = gtk_check_button_new_with_label (_("Show again on startup"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgetThreeButton), data->show_intro_on_startup);
    gtk_grid_attach (GTK_GRID (widgetThree), widgetThreeText, 0, 0, 1, 1);
    gtk_grid_attach_next_to (GTK_GRID (widgetThree), widgetThreeButton, widgetThreeText, GTK_POS_BOTTOM, 1, 1);
    g_signal_connect (G_OBJECT (widgetThreeButton), "toggled",
		      G_CALLBACK (on_intro_show_again_button_toggled), data);
    gtk_assistant_append_page (GTK_ASSISTANT (assistant), widgetThree);
    gtk_assistant_set_page_type (GTK_ASSISTANT (assistant), widgetThree, GTK_ASSISTANT_PAGE_CONFIRM);
    gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), widgetThree, TRUE);

    // connect the close buttons
    g_signal_connect (G_OBJECT (assistant), "cancel",
		      G_CALLBACK (gtk_widget_destroy), NULL);
    g_signal_connect (G_OBJECT (assistant), "close",
		      G_CALLBACK (gtk_widget_destroy), NULL);

    // show
    gtk_widget_show_all (assistant);
}

void on_edit_config(GtkMenuItem *menuitem,
                    gpointer user_data)
{
    /*
      Check if user config does not exist or is empty.
      If so, copy system config to user config.
    */
    gchar *user_config_path = g_strjoin (G_DIR_SEPARATOR_S, g_get_user_config_dir(), "gromit-mpx.cfg", NULL);
    GFile *user_config_file = g_file_new_for_path(user_config_path);

    guint64 user_config_size = 0;
    GFileInfo *user_config_info = g_file_query_info(user_config_file, G_FILE_ATTRIBUTE_STANDARD_SIZE, 0, NULL, NULL);
    if (user_config_info != NULL) {
        user_config_size = g_file_info_get_size(user_config_info);
        g_object_unref(user_config_info);
    }

    if (!g_file_query_exists(user_config_file, NULL) || user_config_size == 0) {
        g_print("User config does not exist or is empty, copying system config\n");

        gchar *system_config_path = g_strjoin (G_DIR_SEPARATOR_S, SYSCONFDIR, "gromit-mpx", "gromit-mpx.cfg", NULL);
        GFile *system_config_file = g_file_new_for_path(system_config_path);

        GError *error = NULL;
        gboolean result = g_file_copy(system_config_file, user_config_file, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &error);
        if (!result) {
            g_printerr("Error copying system config to user config: %s\n", error->message);
            g_error_free(error);
        }

        g_object_unref(system_config_file);
        g_free(system_config_path);
    }


    /*
      Open user config for editing.
     */
    gchar *user_config_uri = g_strjoin (G_DIR_SEPARATOR_S, "file://", user_config_path, NULL);

    gtk_show_uri_on_window (NULL,
			    user_config_uri,
			    GDK_CURRENT_TIME,
			    NULL);

    /*
      Clean up
     */
    g_object_unref(user_config_file);
    g_free(user_config_path);
    g_free(user_config_uri);
}


void on_issues(GtkMenuItem *menuitem,
               gpointer user_data)
{
    gtk_show_uri_on_window (NULL,
			    "https://github.com/bk138/gromit-mpx/issues",
			    GDK_CURRENT_TIME,
			    NULL);
}


void on_support_liberapay(GtkMenuItem *menuitem, gpointer user_data)
{
    gtk_show_uri_on_window (NULL,
			    "https://liberapay.com/bk138",
			    GDK_CURRENT_TIME,
			    NULL);

}

void on_support_patreon(GtkMenuItem *menuitem, gpointer user_data)
{
    gtk_show_uri_on_window (NULL,
			    "https://patreon.com/bk138",
			    GDK_CURRENT_TIME,
			    NULL);

}

void on_support_paypal(GtkMenuItem *menuitem, gpointer user_data)
{
    gtk_show_uri_on_window (NULL,
			    "https://www.paypal.com/donate?hosted_button_id=N7GSSPRPUSTPU",
			    GDK_CURRENT_TIME,
			    NULL);

}

void on_signal(int signum) {
    // for now only SIGINT and SIGTERM
    gtk_main_quit();
}

