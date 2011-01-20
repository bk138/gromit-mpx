

#ifndef INPUT_H
#define INPUT_H

#include "gromit-mpx.h"

void setup_input_devices (GromitData *data);
void release_grab (GromitData *data, GdkDevice *dev);
void acquire_grab (GromitData *data, GdkDevice *dev);
void toggle_grab  (GromitData *data, GdkDevice *dev);


#endif
