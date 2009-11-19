/* 
 *  Gromit -- a program for painting on the screen
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

#include <string.h>
#include "gromit-mpx.h"
#include "callbacks.h"

gboolean event_expose (GtkWidget *widget,
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



gboolean event_configure (GtkWidget *widget,
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




void event_monitors_changed ( GdkScreen *screen,
			      gpointer   user_data) 
{
  GromitData *data = (GromitData *) user_data;

  if(data->debug)
    g_printerr("DEBUG: screen size changed!\n");

    
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

  
  gtk_widget_realize (data->win); 

  gtk_selection_owner_set (data->win, GA_DATA, GDK_CURRENT_TIME);
  gtk_selection_add_target (data->win, GA_DATA, GA_TOGGLEDATA, 1007);




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
  g_signal_connect (data->screen,"monitors_changed",
		    G_CALLBACK (event_monitors_changed), data);
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
 
  g_signal_connect (data->win, "selection_get",
		    G_CALLBACK (mainapp_event_selection_get), data);
  g_signal_connect (data->win, "selection_received",
		    G_CALLBACK (mainapp_event_selection_received), data);

  gtk_widget_set_extension_events (data->area, GDK_EXTENSION_EVENTS_ALL);
 


  gtk_container_add (GTK_CONTAINER (data->win), data->area);

  gtk_widget_shape_combine_mask (data->win, data->shape, 0,0);

  gtk_widget_show_all (data->area);

}




void clientapp_event_selection_get (GtkWidget          *widget,
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


void clientapp_event_selection_received (GtkWidget *widget,
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



gboolean paint (GtkWidget *win, GdkEventButton *ev, gpointer user_data)
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


gboolean paintto (GtkWidget *win,
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


gboolean paintend (GtkWidget *win, GdkEventButton *ev, gpointer user_data)
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



gboolean proximity_in (GtkWidget *win, GdkEventProximity *ev, gpointer user_data)
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


gboolean proximity_out (GtkWidget *win, GdkEventProximity *ev, gpointer user_data)
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

