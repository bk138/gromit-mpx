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
#include "gromit-mpx.h"
#include "callbacks.h"

gboolean event_expose (GtkWidget *widget,
		       GdkEventExpose *event,
		       gpointer user_data)
{
  GromitData *data = (GromitData *) user_data;

  if(data->debug)
    g_printerr("DEBUG: got expose event\n");

  /*gdk_cairo_set_source_pixmap (data->shape_gc,
			       data->pixmap,
			       event->area.x, 
			       event->area.y);
			       cairo_paint(data->shape_gc);*/

  /*gdk_draw_drawable (gtk_widget_get_window(data->area),
                     gtk_widget_get_style(data->area)->fg_gc[gtk_widget_get_state(data->area)],
                     data->pixmap,
                     event->area.x, event->area.y,
                     event->area.x, event->area.y,
                     event->area.width, event->area.height);*/
  return TRUE;
}



gboolean event_configure (GtkWidget *widget,
			  GdkEventExpose *event,
			  gpointer user_data)
{
  GromitData *data = (GromitData *) user_data;

  if(data->debug)
    g_printerr("DEBUG: got configure event\n");

  data->pixmap = gdk_pixmap_new (gtk_widget_get_window(data->area), data->width,
                                 data->height, -1);

  
  cairo_t *cr = gdk_cairo_create(data->pixmap);
  cairo_rectangle(cr, 0, 0, data->width, data->height);
  cairo_fill(cr);
  cairo_destroy (cr);

  //gdk_draw_rectangle (data->pixmap, gtk_widget_get_style(data->area)->black_gc,
  //                    1, 0, 0, data->width, data->height);
  gdk_window_set_transient_for (gtk_widget_get_window(data->area), gtk_widget_get_window(data->win));

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
  gtk_widget_set_size_request(GTK_WIDGET (data->win), data->width, data->height);
  /* gtk_widget_set_uposition (GTK_WIDGET (data->win), 0, 0); */
  
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
  data->shape_gc = gdk_cairo_create (data->shape);
  //data->shape_gc = gdk_gc_new (data->shape);
  //data->shape_gcv = g_malloc (sizeof (GdkGCValues));
  //gdk_gc_get_values (data->shape_gc, data->shape_gcv);

  //FIXME
  //data->transparent = gdk_color_copy (&(data->shape_gcv->foreground));
  //data->opaque = gdk_color_copy (&(data->shape_gcv->background));
  //  cairo_set_source_rgba(data->shape_gc, 0,0,0,0);
  

  gdk_cairo_set_source_pixmap (data->shape_gc,
			       data->transparent_pixmap,
			       0, 
			       0);
  cairo_paint(data->shape_gc);

  
  //gdk_gc_set_foreground (data->shape_gc, data->transparent);

  //cairo_rectangle(data->shape_gc, 0, 0, data->width, data->height);
  //cairo_fill(data->shape_gc);
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

  if (gtk_selection_data_get_target(selection_data) == GA_TOGGLEDATA)
    {
      ans = data->clientdata;
    }
    
  gtk_selection_data_set (selection_data,
                          gtk_selection_data_get_target(selection_data),
                          8, (guchar*)ans, strlen (ans));
}


void clientapp_event_selection_received (GtkWidget *widget,
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



gboolean paint (GtkWidget *win, GdkEventButton *ev, gpointer user_data)
{
  GromitData *data = (GromitData *) user_data;
  gdouble pressure = 0.5;
  /* get the data for this device */
  GromitDeviceData *devdata = g_hash_table_lookup(data->devdatatable, ev->device);

  if(data->debug)
    g_printerr("DEBUG: Device '%s': Button %i Down at (x,y)=(%.2f : %.2f)\n", 
	       gdk_device_get_name(ev->device), ev->button, ev->x, ev->y);

  if (!devdata->is_grabbed)
    return FALSE;

 
  /* See GdkModifierType. Am I fixing a Gtk misbehaviour???  */
  ev->state |= 1 << (ev->button + 7);


  if (ev->state != devdata->state)
    gromit_select_tool (data, ev->device, ev->state);

  gdk_window_set_background (gtk_widget_get_window(data->area),
                             devdata->cur_context->fg_color);

  devdata->lastx = ev->x;
  devdata->lasty = ev->y;
  devdata->motion_time = ev->time;

  if (gdk_device_get_source(ev->device) == GDK_SOURCE_MOUSE)
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
  guint nevents;
  int i;
  gboolean ret;
  gdouble pressure = 0.5;
  /* get the data for this device */
  GromitDeviceData *devdata = g_hash_table_lookup(data->devdatatable, ev->device);

  if (!devdata->is_grabbed)
    return FALSE;
 
  if (ev->state != devdata->state)
    gromit_select_tool (data, ev->device, ev->state);

  ret = gdk_device_get_history (ev->device, ev->window,
                                devdata->motion_time, ev->time,
                                &coords, &nevents);

  if(!data->xinerama && nevents > 0)
    {
      for (i=0; i < nevents; i++)
        {
          gdouble x, y;

          gdk_device_get_axis (ev->device, coords[i]->axes,
                               GDK_AXIS_PRESSURE, &pressure);
          if (pressure > 0)
            {
              if (gdk_device_get_source(ev->device) == GDK_SOURCE_MOUSE)
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
      if (gdk_device_get_source(ev->device) == GDK_SOURCE_MOUSE)
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

  gdk_window_get_pointer (gtk_widget_get_window(data->win), &x, &y, &state);
  gromit_select_tool (data, ev->device, state);

  if(data->debug)
    g_printerr("DEBUG: prox in device  %s: \n", gdk_device_get_name(ev->device));
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
    g_printerr("DEBUG: prox out device  %s: \n", gdk_device_get_name(ev->device));
  return FALSE;
}


/* Remote control */
void mainapp_event_selection_get (GtkWidget          *widget,
				  GtkSelectionData   *selection_data,
				  guint               info,
				  guint               time,
				  gpointer            user_data)
{
  GromitData *data = (GromitData *) user_data;
  
  gchar *uri = "OK";

  if(gtk_selection_data_get_target(selection_data) == GA_TOGGLE)
    {
      /* ask back client for device id */
      gtk_selection_convert (data->win, GA_DATA,
                             GA_TOGGLEDATA, time);
      gtk_main(); /* Wait for the response */
    }
  else if (gtk_selection_data_get_target(selection_data) == GA_VISIBILITY)
    gromit_toggle_visibility (data);
  else if (gtk_selection_data_get_target(selection_data) == GA_CLEAR)
    gromit_clear_screen (data);
  else if (gtk_selection_data_get_target(selection_data) == GA_RELOAD)
    setup_input_devices(data);
  else if (gtk_selection_data_get_target(selection_data) == GA_QUIT)
    gtk_main_quit ();
  else
    uri = "NOK";

   
  gtk_selection_data_set (selection_data,
                          gtk_selection_data_get_target(selection_data),
                          8, (guchar*)uri, strlen (uri));
}


void mainapp_event_selection_received (GtkWidget *widget,
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
	  int dev_nr = strtoull((gchar*)gtk_selection_data_get_data(selection_data), NULL, 10);
	  
          if(data->debug)
            g_printerr("DEBUG: mainapp got toggle id '%d' back from client.\n", dev_nr);

	  if(dev_nr < 0)
	    gromit_toggle_grab(data, NULL); /* toggle all */
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
		gromit_toggle_grab(data, devdata->device);
	      else
		g_printerr("ERROR: No device at index %d.\n", dev_nr);
	    }
        }
    }
 
  gtk_main_quit ();
}


void device_removed_cb (GdkDeviceManager *device_manager,
			GdkDevice        *device,
			gpointer          user_data)
{
  GromitData *data = (GromitData *) user_data;
    
  if(!gdk_device_get_device_type(device) == GDK_DEVICE_TYPE_MASTER
     || gdk_device_get_n_axes(device) < 2)
    return;
  
  if(data->debug)
    g_printerr("DEBUG: device '%s' removed\n", gdk_device_get_name(device));

  setup_input_devices(data);
}

void device_added_cb (GdkDeviceManager *device_manager,
		      GdkDevice        *device,
		      gpointer          user_data)
{
  GromitData *data = (GromitData *) user_data;

  if(!gdk_device_get_device_type(device) == GDK_DEVICE_TYPE_MASTER
     || gdk_device_get_n_axes(device) < 2)
    return;

  if(data->debug)
    g_printerr("DEBUG: device '%s' added\n", gdk_device_get_name(device));

  setup_input_devices(data);
}

