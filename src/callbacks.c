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

  devdata->lastx = ev->x;
  devdata->lasty = ev->y;
  devdata->motion_time = ev->time;

  snap_undo_state (data);

  gdk_event_get_axis ((GdkEvent *) ev, GDK_AXIS_PRESSURE, &pressure);
  data->maxwidth = (CLAMP (pressure + line_thickener, 0, 1) *
		    (double) (devdata->cur_context->width -
			      devdata->cur_context->minwidth) +
		    devdata->cur_context->minwidth);

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

  gdk_device_get_history (ev->device, ev->window,
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
	      data->maxwidth = (CLAMP (pressure + line_thickener, 0, 1) *
				(double) (devdata->cur_context->width -
					  devdata->cur_context->minwidth) +
				devdata->cur_context->minwidth);

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

      if(devdata->motion_time > 0)
	{
	  draw_line (data, ev->device, devdata->lastx, devdata->lasty, ev->x, ev->y);
	  coord_list_prepend (data, ev->device, ev->x, ev->y, data->maxwidth);
	}
    }

  devdata->lastx = ev->x;
  devdata->lasty = ev->y;
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

  gfloat direction = 0;
  gint width = 0;
  if(devdata->cur_context)
    width = devdata->cur_context->arrowsize * devdata->cur_context->width / 2;
   

  if ((ev->x != devdata->lastx) ||
      (ev->y != devdata->lasty))
    on_motion(win, (GdkEventMotion *) ev, user_data);

  if (!devdata->is_grabbed)
    return FALSE;
  
  if (devdata->cur_context->arrowsize != 0 &&
      coord_list_get_arrow_param (data, ev->device, width * 3,
				  &width, &direction))
    draw_arrow (data, ev->device, ev->x, ev->y, width, direction);

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
    
  if(!gdk_device_get_device_type(device) == GDK_DEVICE_TYPE_MASTER
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

  if(!gdk_device_get_device_type(device) == GDK_DEVICE_TYPE_MASTER
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
    const gchar *authors [] = { "Christian Beier <dontmind@freeshell.org>",
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
                                 NULL };
    gtk_show_about_dialog (NULL,
			   "program-name", "Gromit-MPX",
			   "logo-icon-name", "net.christianbeier.Gromit-MPX",
			   "title", "About Gromit-MPX",
			   "comments", "Gromit-MPX (GRaphics Over MIscellaneous Things - Multi-Pointer-EXtension) is an on-screen annotation tool that works with any Unix desktop environment under X11 as well as Wayland.",
			   "version", VERSION,
			   "website", "https://github.com/bk138/gromit-mpx",
			   "authors", authors,
			   "copyright", "2009-2022 Christian Beier, Copyright 2000 Simon Budig",
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
    GtkWidget *widgetOne = gtk_label_new ("Gromit-MPX (GRaphics Over MIscellaneous Things) is a small tool to make\n"
					  "annotations on the screen.\n\n"
					  "Its main use is for making presentations of some application. Normally,\n"
					  "you would have to move the mouse pointer around the point of interest\n"
					  "until hopefully everybody noticed it.  With Gromit-MPX, you can draw\n"
					  "everywhere onto the screen, highlighting some button or area.\n\n"
                                          "If you happen to enjoy using Gromit-MPX, please consider supporting\n"
					  "its development by using one of the donation options on the project's\n"
					  "website or directly via the support options available from the tray menu.\n");
    gtk_assistant_append_page (GTK_ASSISTANT (assistant), widgetOne);
    gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), widgetOne, "Gromit-MPX - What is it?");
    gtk_assistant_set_page_type (GTK_ASSISTANT (assistant), widgetOne, GTK_ASSISTANT_PAGE_INTRO);
    gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), widgetOne, TRUE);

    // set page two
    GtkWidget *widgetTwo = gtk_label_new (NULL);
    char widgetTwoBuf[4096];
    snprintf(widgetTwoBuf, sizeof(widgetTwoBuf),
	     "You can operate Gromit-MPX using its tray icon (if your desktop environment\n"
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
	     "   redo last undone stroke: SHIFT-%s</b></tt>",
	     data->hot_keyval, data->undo_keyval,
	     data->hot_keyval, data->hot_keyval, data->hot_keyval, data->hot_keyval, data->undo_keyval, data->undo_keyval);
    gtk_label_set_markup (GTK_LABEL (widgetTwo), widgetTwoBuf);
    gtk_assistant_append_page (GTK_ASSISTANT (assistant), widgetTwo);
    gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), widgetTwo, "Gromit-MPX - How to use it");
    gtk_assistant_set_page_type (GTK_ASSISTANT (assistant), widgetTwo, GTK_ASSISTANT_PAGE_CONTENT);
    gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), widgetTwo, TRUE);

    // set page three
    GtkWidget *widgetThree = gtk_grid_new ();
    GtkWidget *widgetThreeText = gtk_label_new ("Do you want to show this introduction again on the next start of Gromit-MPX?\n"
						"You can always access it again via the sys tray menu.\n");
    GtkWidget *widgetThreeButton = gtk_check_button_new_with_label ("Show again on startup");
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
