
/*
 * Functions for setting up (parts of) the application
 */

#include <string.h>
#include <X11/extensions/XInput2.h>

#include "input.h"


void setup_input_devices (GromitData *data)
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

  devices = g_list_concat(gdk_device_manager_list_devices
                          (device_manager, GDK_DEVICE_TYPE_MASTER),
                          gdk_device_manager_list_devices
                          (device_manager, GDK_DEVICE_TYPE_SLAVE));
  for(d = devices; d; d = d->next)
    {
      GdkDevice *device = (GdkDevice *) d->data;
    
      /* only enable devices with 2 ore more axes */
      if (gdk_device_get_source(device) != GDK_SOURCE_KEYBOARD && gdk_device_get_n_axes(device) >= 2)
        {
          gdk_device_set_mode (device, GDK_MODE_SCREEN);

          GromitDeviceData *devdata;

          devdata  = g_malloc0(sizeof (GromitDeviceData));
          devdata->device = device;
          devdata->index = i;

	  /* get attached keyboard and grab the hotkey */
	  if (!data->hot_keycode || !data->undo_keycode)
	    {
	      g_printerr("ERROR: Grabbing hotkey from attached keyboard "
                         "of '%s' failed, no hotkeys defined.\n",
			 gdk_device_get_name(device));
	      g_free(devdata);
	      continue;
	    }

          /* if this is a slave device, we need the master */
          GdkDevice *kdevice=device;
          if(gdk_device_get_device_type (device) == GDK_DEVICE_TYPE_SLAVE)
            kdevice=gdk_device_get_associated_device (device);

	  gint dev_id = -1;
	  g_object_get(kdevice, "device-id", &dev_id, NULL);

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
		g_printerr("DEBUG: Grabbing hotkeys from keyboard '%d' .\n", kbd_dev_id);

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

	      XIGrabKeycode( GDK_DISPLAY_XDISPLAY(data->display),
			     kbd_dev_id,
			     data->undo_keycode,
			     GDK_WINDOW_XID(data->root),
			     GrabModeAsync,
			     GrabModeAsync,
			     True,
			     &mask,
			     nmods,
			     modifiers);

	      XSync(GDK_DISPLAY_XDISPLAY(data->display), False);
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



void release_grab (GromitData *data, 
		   GdkDevice *dev)
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
	    gdk_error_trap_push();
	    gdk_device_ungrab(devdata->device, GDK_CURRENT_TIME);
	    XSync(GDK_DISPLAY_XDISPLAY(data->display), False);
	    if(gdk_error_trap_pop())
	      g_printerr("WARNING: Ungrabbing device '%s' failed.\n", gdk_device_get_name(devdata->device));

	    devdata->is_grabbed = 0;
            /* workaround buggy GTK3 ? */
	    devdata->motion_time = 0;
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
      /* workaround buggy GTK3 ? */
      devdata->motion_time = 0;


      if(data->debug)
        g_printerr ("DEBUG: Ungrabbed Device '%s'.\n", gdk_device_get_name(devdata->device));
    }

  if (!data->painted)
    hide_window (data);
}


void acquire_grab (GromitData *data, 
		   GdkDevice *dev)
{
  show_window (data);

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
          if(devdata->is_grabbed || gdk_device_get_device_type(devdata->device) == GDK_DEVICE_TYPE_SLAVE)
            continue;

	  if(devdata->cur_context && devdata->cur_context->type == GROMIT_ERASER)
	    cursor = data->erase_cursor; 
	  else
	    cursor = data->paint_cursor; 

	
	  if(gdk_device_grab(devdata->device,
			     gtk_widget_get_window(data->win),
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

  GromitDeviceData *devdata =
    g_hash_table_lookup(data->devdatatable, dev);
  if (!devdata->is_grabbed)
    {
      GdkCursor *cursor;
      if(devdata->cur_context && devdata->cur_context->type == GROMIT_ERASER)
        cursor = data->erase_cursor; 
      else
        cursor = data->paint_cursor; 
      
      if(gdk_device_grab(devdata->device,
                         gtk_widget_get_window(data->win),
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


void toggle_grab (GromitData *data, 
		  GdkDevice* dev)
{
  if(dev == NULL) /* toggle all */
    {
      if (data->all_grabbed)
	release_grab (data, NULL);
      else
	acquire_grab (data, NULL);
      return;
    }

  GromitDeviceData *devdata =
    g_hash_table_lookup(data->devdatatable, dev);
  if(devdata)
    {
      if(devdata->is_grabbed)
        release_grab (data, devdata->device);
      else
        acquire_grab (data, devdata->device);
    }
  else
    g_printerr("ERROR: No such device '%s' in internal table.\n", gdk_device_get_name(dev));
}
