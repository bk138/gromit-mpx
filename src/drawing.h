#ifndef DRAWING_H
#define DRAWING_H

/*
  Functions that manipulate the surfaces pixel-wise.
  Does not include functions that treat a surface like a buffer, like undo/redo etc.
*/

#include "main.h"

typedef struct
{
  gint x;
  gint y;
  gint width;
} GromitStrokeCoordinate;


void draw_line (GromitData *data, GdkDevice *dev, gint x1, gint y1, gint x2, gint y2);
void draw_arrow (GromitData *data, GdkDevice *dev, gint x1, gint y1, gint width, gfloat direction);

#endif
