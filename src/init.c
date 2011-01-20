
/*
 * Functions for setting up (parts of) the application
 */

#include <string.h>
#include <X11/extensions/XInput2.h>

#include "callbacks.h"
#include "init.h"




void init_input_devices (GromitData *data)
{
  /* ungrab all */
  release_grab (data, NULL); 

  /* and clear our own device data list */
  GHashTableIter it;
  gpointer value;
  g_hash_table_iter_init (&it, data->devdatatable);
  while (g_hash_table_iter_next (&it, NULL, &value)) 
    g_free(value);
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

	  gint dev_id = -1;
	  g_object_get(device, "device-id", &dev_id, NULL);

	  gint kbd_dev_id = -1;
	  XIDeviceInfo* devinfo;
	  int devicecount = 0;
	  
	  devinfo = XIQueryDevice(GDK_DISPLAY_XDISPLAY(data->display),
				  dev_id,
				  &devicecount);
	  if(devicecount)
	    kbd_dev_id = devinfo->attachment;
	  XIFreeDeviceInfo(devinfo);
	  
 
	  if(kbd_dev_id != -1)
	    {
	      if(data->debug)
		g_printerr("DEBUG: Grabbing hotkey from keyboard '%d' .\n", kbd_dev_id);

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
			     GDK_WINDOW_XID(data->root),
			     GrabModeAsync,
			     GrabModeAsync,
			     True,
			     &mask,
			     nmods,
			     modifiers);

	      gdk_flush ();
	      if(gdk_error_trap_pop())
		{
		  g_printerr("ERROR: Grabbing hotkey from keyboard device %d failed.\n",
			     kbd_dev_id);
		  g_free(devdata);
		  continue;
		}
	    }
	    

         
	  g_hash_table_insert(data->devdatatable, device, devdata);
          g_printerr ("Enabled Device %d: \"%s\", (Type: %d)\n", 
		      i++, gdk_device_get_name(device), gdk_device_get_source(device));
        }
    }

  g_printerr ("Now %d enabled devices.\n", g_hash_table_size(data->devdatatable));
}




/*
  this is shared by the remote control and 
  real drawing app
*/
void init_basic_stuff (GromitData *data)
{
  data->display = gdk_display_get_default ();
  data->screen = gdk_display_get_default_screen (data->display);
  data->xinerama = gdk_screen_get_n_monitors (data->screen) > 1;
  data->root = gdk_screen_get_root_window (data->screen);
  data->width = gdk_screen_get_width (data->screen);
  data->height = gdk_screen_get_height (data->screen);

  g_object_unref(data->win);
  data->win = gtk_window_new (GTK_WINDOW_POPUP);
  g_object_ref(data->win);

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
		    G_CALLBACK (on_clientapp_selection_received), data);
  g_signal_connect (data->win, "selection_get",
		    G_CALLBACK (on_clientapp_selection_get), data);
  
  gtk_widget_realize (data->win); 

  gtk_selection_owner_set (data->win, GA_DATA, GDK_CURRENT_TIME);
  gtk_selection_add_target (data->win, GA_DATA, GA_TOGGLEDATA, 1007);
}


void init_colors(GromitData *data)
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
}


void init_canvas(GromitData *data)
{
  /* SHAPE SURFACE*/
  cairo_surface_destroy(data->shape);
  data->shape = cairo_image_surface_create(CAIRO_FORMAT_ARGB32 ,data->width, data->height);

   
  /* DRAWING AREA */
  g_object_unref (data->area);
  data->area = gtk_drawing_area_new ();
  g_object_ref (data->area);
  gtk_widget_set_size_request(GTK_WIDGET(data->area),
			      data->width, data->height);

  
  /* EVENTS */
  gtk_widget_set_events (data->area, GROMIT_PAINT_AREA_EVENTS);
  g_signal_connect (data->area, "expose_event",
		    G_CALLBACK (on_expose), data);
  g_signal_connect (data->area,"configure_event",
		    G_CALLBACK (on_configure), data);
  g_signal_connect (data->screen,"monitors_changed",
		    G_CALLBACK (on_monitors_changed), data);
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
  g_signal_connect (data->win, "proximity_in_event",
		    G_CALLBACK (on_proximity_in), data);
  g_signal_connect (data->win, "proximity_out_event",
		    G_CALLBACK (on_proximity_out), data);
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



  gtk_container_add (GTK_CONTAINER (data->win), data->area);

  
  cairo_region_t* r = gdk_cairo_region_create_from_surface(data->shape);
  gtk_widget_shape_combine_region(data->win, r);
  cairo_region_destroy(r);
  

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

}
