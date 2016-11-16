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
#include "input.h"
#include "callbacks.h"
#include "config.h"
#include "build-config.h"


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

  if(data->debug)
    g_printerr("DEBUG: screen size changed!\n");

  // get new sizes
  data->width = gdk_screen_get_width (data->screen);
  data->height = gdk_screen_get_height (data->screen);

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


  data->default_pen = paint_context_new (data, GROMIT_PEN,
					 data->red, 7, 0, 1);
  data->default_eraser = paint_context_new (data, GROMIT_ERASER,
					    data->red, 75, 0, 1);

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

  if (gtk_selection_data_get_target(selection_data) == GA_TOGGLEDATA)
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
  GdkDevice *master = ev->device;
  GromitDeviceData *masterdata =
    g_hash_table_lookup(data->devdatatable, master);
  GdkDevice *slave =
    gdk_event_get_source_device ((GdkEvent *) ev);
  GromitDeviceData *slavedata =
    g_hash_table_lookup(data->devdatatable, slave);

  if(data->debug)
    g_printerr("DEBUG: Device '%s': Button %i Down at (x,y)=(%.2f : %.2f)\n", 
	       gdk_device_get_name(slave), ev->button, ev->x, ev->y);

  if (!masterdata->is_grabbed)
    return FALSE;

  /* See GdkModifierType. Am I fixing a Gtk misbehaviour???  */
  ev->state |= 1 << (ev->button + 7);

  if (ev->state != masterdata->state ||
      ev->state != slavedata->state ||
      masterdata->lastslave != slave)
    select_tool (data, master, slave, ev->state);

  slavedata->lastx = ev->x;
  slavedata->lasty = ev->y;
  slavedata->motion_time = ev->time;

  snap_undo_state (data);

  gdk_event_get_axis ((GdkEvent *) ev, GDK_AXIS_PRESSURE, &pressure);
  data->maxwidth = (CLAMP (pressure + line_thickener, 0, 1) *
		    (double) (slavedata->cur_context->width -
			      slavedata->cur_context->minwidth) +
		    slavedata->cur_context->minwidth);

  if (ev->button <= 5)
    draw_line (data, slave, ev->x, ev->y, ev->x, ev->y);

  coord_list_prepend (data, slave, ev->x, ev->y, data->maxwidth);

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
  GdkDevice *master = ev->device;
  GromitDeviceData *masterdata =
    g_hash_table_lookup(data->devdatatable, master);

  if (!masterdata->is_grabbed)
    return FALSE;

  GdkDevice *slave =
    gdk_event_get_source_device ((GdkEvent *) ev);
  GromitDeviceData *slavedata =
    g_hash_table_lookup(data->devdatatable, slave);

  if(data->debug)
      g_printerr("DEBUG: Device '%s': motion to (x,y)=(%.2f : %.2f)\n", gdk_device_get_name(slave), ev->x, ev->y);

  if (ev->state != masterdata->state ||
      ev->state != slavedata->state ||
      masterdata->lastslave != slave)
    select_tool (data, master, slave, ev->state);

  if(!(ev->state & (GDK_BUTTON1_MASK|
                    GDK_BUTTON2_MASK|
                    GDK_BUTTON3_MASK|
                    GDK_BUTTON4_MASK|
                    GDK_BUTTON5_MASK)))
    return TRUE;

  gdk_device_get_history (slave, ev->window,
                          slavedata->motion_time, ev->time,
                          &coords, &nevents);

  if(!data->xinerama && nevents > 0)
    {
      for (i=0; i < nevents; i++)
        {
          gdouble x, y;

          gdk_device_get_axis (slave, coords[i]->axes,
                               GDK_AXIS_PRESSURE, &pressure);
          if (pressure > 0)
            {
	      data->maxwidth = (CLAMP (pressure + line_thickener, 0, 1) *
				(double) (slavedata->cur_context->width -
					  slavedata->cur_context->minwidth) +
				slavedata->cur_context->minwidth);

              gdk_device_get_axis(slave, coords[i]->axes,
                                  GDK_AXIS_X, &x);
              gdk_device_get_axis(slave, coords[i]->axes,
                                  GDK_AXIS_Y, &y);

	      draw_line (data, slave, slavedata->lastx, slavedata->lasty, x, y);

              coord_list_prepend (data, slave, x, y, data->maxwidth);
              slavedata->lastx = x;
              slavedata->lasty = y;
            }
        }

      slavedata->motion_time = coords[nevents-1]->time;
      g_free (coords);
    }

  /* always paint to the current event coordinate. */
  gdk_event_get_axis ((GdkEvent *) ev, GDK_AXIS_PRESSURE, &pressure);

  if (pressure > 0)
    {
      data->maxwidth = (CLAMP (pressure + line_thickener, 0, 1) *
			(double) (slavedata->cur_context->width -
				  slavedata->cur_context->minwidth) +
			slavedata->cur_context->minwidth);

      if(slavedata->motion_time > 0)
	{
	  draw_line (data, slave, slavedata->lastx, slavedata->lasty, ev->x, ev->y);
	  coord_list_prepend (data, slave, ev->x, ev->y, data->maxwidth);
	}
    }

  slavedata->lastx = ev->x;
  slavedata->lasty = ev->y;
  slavedata->motion_time = ev->time;

  return TRUE;
}


gboolean on_buttonrelease (GtkWidget *win, 
			   GdkEventButton *ev, 
			   gpointer user_data)
{
  GromitData *data = (GromitData *) user_data;

  /* get the data for this device */
  GdkDevice *master = ev->device;
  GromitDeviceData *masterdata =
    g_hash_table_lookup(data->devdatatable, master);
  GdkDevice *slave =
    gdk_event_get_source_device ((GdkEvent *) ev);
  GromitDeviceData *slavedata =
    g_hash_table_lookup(data->devdatatable, slave);

  if(data->debug)
    g_printerr("DEBUG: Device '%s': Button %i Up at (x,y)=(%.2f : %.2f)\n", 
	       gdk_device_get_name(slave), ev->button, ev->x, ev->y);

  gfloat direction = 0;
  gint width = 0;
  if(slavedata->cur_context)
    width = slavedata->cur_context->arrowsize *
      slavedata->cur_context->width / 2;

  if ((ev->x != slavedata->lastx) ||
      (ev->y != slavedata->lasty))
    on_motion(win, (GdkEventMotion *) ev, user_data);


  if (!masterdata->is_grabbed)
    return FALSE;

  if (slavedata->cur_context->arrowsize != 0 &&
      coord_list_get_arrow_param (data, slave, width * 3,
				  &width, &direction))
    draw_arrow (data, slave, ev->x, ev->y, width, direction);

  coord_list_free (data, slave);

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
    }
 
  gtk_main_quit ();
}


void on_device_removed (GdkDeviceManager *device_manager,
			GdkDevice        *device,
			gpointer          user_data)
{
  GromitData *data = (GromitData *) user_data;
  GdkInputSource hardware_type = gdk_device_get_source(device);

  if( hardware_type == GDK_SOURCE_KEYBOARD ||
      gdk_device_get_n_axes(device) < 2)
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
  GdkInputSource hardware_type = gdk_device_get_source(device);

  if( hardware_type == GDK_SOURCE_KEYBOARD ||
      gdk_device_get_n_axes(device) < 2)
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

    GdkDevice *master = ev->device;
    GdkDevice *slave = gdk_event_get_source_device ((GdkEvent *) ev);

    if(data->debug)
	g_printerr("DEBUG: Device '%s': Button %i on_toggle_paint at (x,y)=(%.2f : %.2f)\n",
		   gdk_device_get_name(slave), ev->button, ev->x, ev->y);

    toggle_grab(data, master);

    return TRUE;
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


void on_help(GtkMenuItem *menuitem,
	     gpointer     user_data)
{
    GromitData *data = (GromitData *) user_data;

    gchar helpString[4096];
    snprintf(helpString, 4096,  "The available commands are:\n\n<tt><b>\
    toggle painting:         %s\n\
    clear screen:            SHIFT-%s\n\
    toggle visibility:       CTRL-%s\n\
    quit:                    ALT-%s\n\
    undo last stroke:        %s\n\
    redo last undone stroke: SHIFT-%s</b></tt>",
	     data->hot_keyval, data->hot_keyval, data->hot_keyval, data->hot_keyval,
	     data->undo_keyval, data->undo_keyval);

    GtkWidget *dialog = gtk_message_dialog_new(NULL,
					       GTK_DIALOG_DESTROY_WITH_PARENT,
					       GTK_MESSAGE_INFO,
					       GTK_BUTTONS_CLOSE,
					       NULL);
    gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog), helpString);

    g_signal_connect_swapped (dialog, "response",
			      G_CALLBACK (gtk_widget_destroy),
			      dialog);

    gtk_widget_show_all(dialog);
}



void on_about(GtkMenuItem *menuitem,
	      gpointer     user_data)
{
    const gchar *authors [] = { "Christian Beier <dontmind@freeshell.org>", "Simon Budig <Simon.Budig@unix-ag.org>", NULL };
    gtk_show_about_dialog (NULL,
			   "program-name", "Gromit-MPX",
			   "logo-icon-name", "gromit-mpx",
			   "title", "About Gromit-MPX",
			   "comments", "Gromit-MPX (GRaphics Over MIscellaneous Things) is a small tool to make annotations on the screen. Gromit-MPX is a multi-pointer port of the original Gromit annotation tool by Simon Budig.",
			   "version", VERSION,
			   "website", "https://github.com/bk138/gromit-mpx",
			   "authors", authors,
			   "copyright", "Copyright 2000 Simon Budig, 2009-2016 Christian Beier",
			   "license-type", GTK_LICENSE_GPL_2_0,
			   NULL);
}


