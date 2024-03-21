

#ifndef INPUT_H
#define INPUT_H

#include "main.h"

void setup_input_devices (GromitData *data);
void shutdown_input_devices (GromitData *data);
void release_grab (GromitData *data, GdkDevice *dev);
void acquire_grab (GromitData *data, GdkDevice *dev);
void toggle_grab  (GromitData *data, GdkDevice *dev);
gint snoop_key_press (GtkWidget *grab_widget, GdkEventKey *event, gpointer func_data);

guint grab_key(GromitData *data, gint device_id, const char *key, int num_modifiers, gpointer key_modifiers, gpointer mask);
guint ungrab_key(GromitData *data, gint device_id, const char *key, int num_modifiers, gpointer key_modifiers);

#endif
