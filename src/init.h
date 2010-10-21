
#ifndef INIT_H
#define INIT_H

#include "gromit-mpx.h"

void init_input_devices (GromitData *data);

/*
  this is shared by the remote control and 
  real drawing app
*/
void init_basic_stuff (GromitData *data);

void init_colormaps (GromitData *data);

void init_canvas(GromitData *data);


#endif
