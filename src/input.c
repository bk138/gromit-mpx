
/*
 * Functions for setting up (parts of) the application
 */

#include <string.h>
#include <gdk/gdk.h>
#ifdef GDK_WINDOWING_X11
#include <X11/extensions/XInput2.h>
#include <gdk/gdkx.h>
#endif
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif

#define WAYLAND_HOTKEY_PREFIX "gromit-mpx-wayland-hotkey"

#include "input.h"


static gboolean get_are_all_grabbed(GromitData *data)
{
    // iterate over devices to get grab statuses
    gboolean all_grabbed = FALSE;
    GHashTableIter it;
    gpointer value;
    GromitDeviceData* devdata = NULL;
    g_hash_table_iter_init (&it, data->devdatatable);
    while (g_hash_table_iter_next(&it, NULL, &value)) {
	devdata = value;
	if (devdata->is_grabbed) {
	    all_grabbed = TRUE; // one grabbed device, go on
	    continue;
	} else {
	    all_grabbed = FALSE; // one ungrabbed device, can stop here
	    break;
	}
    }

    return all_grabbed;
}

static gboolean get_are_some_grabbed(GromitData *data)
{
    // iterate over devices to get grab statuses
    gboolean some_grabbed = FALSE;
    GHashTableIter it;
    gpointer value;
    GromitDeviceData* devdata = NULL;
    g_hash_table_iter_init (&it, data->devdatatable);
    while (g_hash_table_iter_next(&it, NULL, &value)) {
	devdata = value;
	if (devdata->is_grabbed) {
	    some_grabbed = TRUE; // one grabbed device, can stop here
	    break;
	}
    }

    return some_grabbed;
}

static void remove_hotkeys_from_compositor(GromitData *data) {
    char *xdg_current_desktop = getenv("XDG_CURRENT_DESKTOP");

    /* don't do anything when no keys are set at all */
    if (!data->hot_keycode && !data->undo_keycode)
	return;

    if (xdg_current_desktop && strstr(xdg_current_desktop, "GNOME") != 0) {
	/*
	  Get all custom key bindings and save back the ones that are not from us.
	*/
	if(data->debug)
	    g_print("DEBUG: Detected GNOME under Wayland, removing our hotkeys from compositor\n");

	GPtrArray *other_key_bindings_mutable_array = g_ptr_array_new();

	GSettings *settings = g_settings_new("org.gnome.settings-daemon.plugins.media-keys");

	gchar **key_bindings_array = g_settings_get_strv(settings, "custom-keybindings");
	gchar **binding;
	for (binding = key_bindings_array; *binding; binding++) {
	    GSettings *settings = g_settings_new_with_path("org.gnome.settings-daemon.plugins.media-keys.custom-keybinding",
							   *binding);
	    gchar *name = g_settings_get_string(settings, "name");

            if (!g_str_has_prefix(name, WAYLAND_HOTKEY_PREFIX)) {
		g_ptr_array_add(other_key_bindings_mutable_array, strdup(*binding));
            } else {
		if (data->debug)
		    g_print("DEBUG:   removing %s with name '%s'\n", *binding, name);
            }

            g_free(name);
	    g_object_unref(settings);
	}

	g_ptr_array_add(other_key_bindings_mutable_array, NULL);
	gchar **other_key_bindings_array = (gchar **)g_ptr_array_free(other_key_bindings_mutable_array, FALSE);
	g_settings_set_strv(settings, "custom-keybindings", (const gchar**)other_key_bindings_array);

	g_strfreev(other_key_bindings_array);
	g_strfreev(key_bindings_array);
	g_object_unref(settings);
    }
}

/**
   Add hotkeys bound to gromit-mpx remote control commands.
   Under (X)Wayland, it is per design not possible to listen for a certain
   hotkey press. While gtk_key_snooper_install() works when an XWayland window
   has keyboard focus, that does not help very much.
   Thus, we have to register hotkeys using each compositor's own API until
   there's maybe a cross-platform way of doing this.
 */
static void add_hotkeys_to_compositor(GromitData *data) {
    char *xdg_current_desktop = getenv("XDG_CURRENT_DESKTOP");

    /* don't do anything when no keys are set at all */
    if (!data->hot_keycode && !data->undo_keycode)
	return;

    if (xdg_current_desktop && strstr(xdg_current_desktop, "GNOME") != 0) {

	if(data->debug)
	    g_print("DEBUG: Detected GNOME under Wayland, adding our hotkeys to compositor\n");

	/*
	  Get highest custom keybinding index, collecting keybindings on the way.
	*/
	guint binding_index = 0;
	GPtrArray *new_key_bindings_mutable_array = g_ptr_array_new();

	GSettings *settings = g_settings_new("org.gnome.settings-daemon.plugins.media-keys");

	gchar **old_key_bindings_array = g_settings_get_strv(settings, "custom-keybindings");
	gchar **binding;
	for (binding = old_key_bindings_array; *binding; binding++) {
	    /* store existing bindings in mutable array */
	    g_ptr_array_add(new_key_bindings_mutable_array, strdup(*binding));

	    /* get the path components */
	    gchar **components = g_strsplit (*binding, "/", 0);
	    /* get the last component. -2 because g_strv_length() counts the trailing NULL as well */
	    gchar *custom_numbered = components[g_strv_length(components)-2];
	    guint custom_index = g_ascii_strtoull (custom_numbered+6, NULL, 10);
	    if(custom_index >= binding_index)
		binding_index = custom_index + 1;

	    g_strfreev(components);
	}

	/*
	  add our keybindings
	 */
	gchar *modifier_hotkey_option[18];
	modifier_hotkey_option[0] = "";
	modifier_hotkey_option[1] = data->hot_keyval;
	modifier_hotkey_option[2] = "--toggle";
	modifier_hotkey_option[3] = "<Shift>";
	modifier_hotkey_option[4] = data->hot_keyval;
	modifier_hotkey_option[5] = "--clear";
	modifier_hotkey_option[6] = "<Ctrl>";
	modifier_hotkey_option[7] = data->hot_keyval;
	modifier_hotkey_option[8] = "--visibility";
	modifier_hotkey_option[9] = "<Alt>";
	modifier_hotkey_option[10] = data->hot_keyval;
	modifier_hotkey_option[11] = "--quit";
	modifier_hotkey_option[12] = "";
	modifier_hotkey_option[13] = data->undo_keyval;
	modifier_hotkey_option[14] = "--undo";
	modifier_hotkey_option[15] = "<Shift>";
	modifier_hotkey_option[16] = data->undo_keyval;
	modifier_hotkey_option[17] = "--redo";

	for(int i = 0; i < 18; i+=3) {
	    gchar *new_binding = g_malloc(128);
	    snprintf(new_binding,
		     128,
		     "/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/custom%d/",
		     binding_index++);
	    g_ptr_array_add(new_key_bindings_mutable_array, new_binding);

	    GSettings *settings = g_settings_new_with_path("org.gnome.settings-daemon.plugins.media-keys.custom-keybinding",
							   new_binding);
	    gchar name[64];
	    snprintf(name, 64, "%s %s", WAYLAND_HOTKEY_PREFIX, modifier_hotkey_option[i+2]);
	    g_settings_set_string(settings, "name", name);

	    gchar command[64];
	    if(getenv("FLATPAK_ID"))
		snprintf(command, 64, "%s %s", "flatpak run net.christianbeier.Gromit-MPX", modifier_hotkey_option[i+2]);
	    else
		snprintf(command, 64, "%s %s", "gromit-mpx", modifier_hotkey_option[i+2]);
	    g_settings_set_string(settings, "command", command);

	    gchar binding[64];
	    snprintf(binding, 64, "%s%s", modifier_hotkey_option[i], modifier_hotkey_option[i+1]);
	    g_settings_set_string(settings, "binding", binding);

	    g_object_unref(settings);
	}

	/*
	  Convert the mutable bindings array into a static array and apply this to the setting.
	 */
	g_ptr_array_add(new_key_bindings_mutable_array, NULL);
	gchar **new_key_bindings_array = (gchar **)g_ptr_array_free(new_key_bindings_mutable_array, FALSE);
	g_settings_set_strv(settings, "custom-keybindings", (const gchar**)new_key_bindings_array);

        g_strfreev(new_key_bindings_array);
        g_strfreev(old_key_bindings_array);
	g_object_unref(settings);
    }
}

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

  devices = gdk_device_manager_list_devices(device_manager, GDK_DEVICE_TYPE_MASTER);
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
	  if (GDK_IS_X11_DISPLAY(data->display)) {
	      gint dev_id = gdk_x11_device_get_id(device);

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
		      XIEventMask mask;
		      unsigned char bits[4] = {0,0,0,0};
		      mask.mask = bits;
		      mask.mask_len = sizeof(bits);
	      
		      XISetMask(bits, XI_KeyPress);
		      XISetMask(bits, XI_KeyRelease);
	      
		      XIGrabModifiers modifiers[] = {{XIAnyModifier, 0}};
		      int nmods = 1;
	      
		      gdk_x11_display_error_trap_push(data->display);
	      
		      if (data->hot_keycode) {
			  if(data->debug)
			      g_printerr("DEBUG: Grabbing hot key '%s' from keyboard '%d' .\n", data->hot_keyval, kbd_dev_id);

			  if(XIGrabKeycode(GDK_DISPLAY_XDISPLAY(data->display),
					   kbd_dev_id,
					   data->hot_keycode,
					   GDK_WINDOW_XID(data->root),
					   GrabModeAsync,
					   GrabModeAsync,
					   True,
					   &mask,
					   nmods,
					   modifiers) != 0) {
			      g_printerr("ERROR: Grabbing hotkey from keyboard device %d failed.\n", kbd_dev_id);
			      GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(data->win),
									 GTK_DIALOG_DESTROY_WITH_PARENT,
									 GTK_MESSAGE_ERROR,
									  GTK_BUTTONS_CLOSE,
									 "Grabbing hotkey %s from keyboard %d failed. The drawing hotkey function will not work unless configured to use another key.",
									 data->hot_keyval,
									 kbd_dev_id);
			      gtk_dialog_run (GTK_DIALOG (dialog));
			      gtk_widget_destroy (dialog);

			  }
		      }

		      if (data->undo_keycode) {
			  if(data->debug)
			      g_printerr("DEBUG: Grabbing undo key '%s' from keyboard '%d' .\n", data->undo_keyval, kbd_dev_id);

			  if(XIGrabKeycode(GDK_DISPLAY_XDISPLAY(data->display),
					   kbd_dev_id,
					   data->undo_keycode,
					   GDK_WINDOW_XID(data->root),
					   GrabModeAsync,
					   GrabModeAsync,
					   True,
					   &mask,
					   nmods,
					   modifiers) != 0) {
			      g_printerr("ERROR: Grabbing undo key from keyboard device %d failed.\n", kbd_dev_id);
			      GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(data->win),
									 GTK_DIALOG_DESTROY_WITH_PARENT,
									 GTK_MESSAGE_ERROR,
									  GTK_BUTTONS_CLOSE,
									 "Grabbing undo key %s from keyboard %d failed. The undo hotkey function will not work unless configured to use another key.",
									 data->undo_keyval,
									 kbd_dev_id);
			      gtk_dialog_run (GTK_DIALOG (dialog));
			      gtk_widget_destroy (dialog);
			  }
		      }

		      XSync(GDK_DISPLAY_XDISPLAY(data->display), False);
		      if(gdk_x11_display_error_trap_pop(data->display))
			  {
			      g_printerr("ERROR: Grabbing keys from keyboard device %d failed due to X11 error.\n",
					 kbd_dev_id);
			      g_free(devdata);
			      continue;
			  }
		  }
	  } // GDK_IS_X11_DISPLAY()

	  /*
	    When running under XWayland, hotkey grabbing does not work and we
	    have to register shortcuts with the compositor.
	   */
	  char *xdg_session_type = getenv("XDG_SESSION_TYPE");
	  if (xdg_session_type && strcmp(xdg_session_type, "wayland") == 0) {
	      remove_hotkeys_from_compositor(data);
	      add_hotkeys_to_compositor(data);
          }

          g_hash_table_insert(data->devdatatable, device, devdata);
          g_printerr ("Enabled Device %d: \"%s\", (Type: %d)\n", 
		      i++, gdk_device_get_name(device), gdk_device_get_source(device));
        }
    }

  g_printerr ("Now %d enabled devices.\n", g_hash_table_size(data->devdatatable));
}

void shutdown_input_devices(GromitData *data)
{
    release_grab(data, NULL); /* ungrab all */
    char *xdg_session_type = getenv("XDG_SESSION_TYPE");
    if (xdg_session_type && strcmp(xdg_session_type, "wayland") == 0)
	remove_hotkeys_from_compositor(data);
}

void release_grab (GromitData *data, 
		   GdkDevice *dev)
{
  char *xdg_session_type = getenv("XDG_SESSION_TYPE");
  if (xdg_session_type && strcmp(xdg_session_type, "wayland") == 0) {
      /*
	When running under Wayland (XWayland in our case),
	make the whole transparent window transparent to input.
      */
      cairo_region_t *r = cairo_region_create();
      gtk_widget_input_shape_combine_region(data->win, r);
      cairo_region_destroy(r);

      // force a redraw, otherwise input shape is not applied,
      // at least on newer GNOME versions
      gdk_window_invalidate_rect(gtk_widget_get_window(data->win), NULL, 0);
  }


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
	    gdk_x11_display_error_trap_push(data->display);
	    gdk_device_ungrab(devdata->device, GDK_CURRENT_TIME);
	    XSync(GDK_DISPLAY_XDISPLAY(data->display), False);
	    if(gdk_x11_display_error_trap_pop(data->display))
	      g_printerr("WARNING: Ungrabbing device '%s' failed.\n", gdk_device_get_name(devdata->device));

	    devdata->is_grabbed = 0;
            /* workaround buggy GTK3 ? */
	    devdata->motion_time = 0;
	  }
        }

      if(data->debug)
        g_printerr ("DEBUG: Ungrabbed all Devices.\n");

      indicate_active(data, FALSE);

      if (!data->painted)
	  hide_window (data);

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

      if(!get_are_some_grabbed(data))
	  indicate_active(data, FALSE);
    }

  if (!data->painted)
    hide_window (data);
}


void acquire_grab (GromitData *data, 
		   GdkDevice *dev)
{
  show_window (data);

  char *xdg_session_type = getenv("XDG_SESSION_TYPE");
  if (xdg_session_type && strcmp(xdg_session_type, "wayland") == 0) {
      /*
	When running under Wayland (XWayland in our case), make the whole transparent window react to input.
	Otherwise, no draw-cursor is shown over Wayland-only windows.
      */
      cairo_rectangle_int_t rect = {0, 0, data->width, data->height};
      cairo_region_t *r = cairo_region_create_rectangle(&rect);
      gtk_widget_input_shape_combine_region(data->win, r);
      cairo_region_destroy(r);
      // force a redraw, otherwise input shape is not applied,
      // at least on newer GNOME versions
      gdk_window_invalidate_rect(gtk_widget_get_window(data->win), NULL, 0);
  }

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

      if(data->debug)
        g_printerr("DEBUG: Grabbed all Devices.\n");

      indicate_active(data, TRUE);

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

      indicate_active(data, TRUE);

    }

}


void toggle_grab (GromitData *data, 
		  GdkDevice* dev)
{
  if(dev == NULL) /* toggle all */
    {
      if (get_are_all_grabbed(data))
	release_grab (data, NULL);
      else
	acquire_grab (data, NULL);
      return; 
    }

  /* get the data for this device */
  GromitDeviceData *devdata = g_hash_table_lookup(data->devdatatable, dev);
      
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



/* Key snooper */
gint snoop_key_press(GtkWidget   *grab_widget,
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
	  toggle_grab(data, gdk_device_get_associated_device(dev));

      return TRUE;
    }
  if (event->type == GDK_KEY_PRESS &&
      event->hardware_keycode == data->undo_keycode)
    {
      if(data->debug)
	g_printerr("DEBUG: Received undokey press from device '%s'\n", gdk_device_get_name(dev));

      if (data->hidden)
        return FALSE;
      if (event->state & GDK_SHIFT_MASK)
        redo_drawing (data);
      else
        undo_drawing (data);

      return TRUE;
    }
  return FALSE;
}
