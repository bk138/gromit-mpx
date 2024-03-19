#ifndef DRAWING_H
#define DRAWING_H

/*
  Functions that manipulate the surfaces pixel-wise.
  Does not include functions that treat a surface like a buffer, like undo/redo etc.
*/

#include "main.h"

void draw_line (GromitData *data, GdkDevice *dev, gint x1, gint y1, gint x2, gint y2);
void draw_arrow (GromitData *data, GdkDevice *dev, gint x1, gint y1, gint width, gfloat direction);

gboolean coord_list_get_arrow_param (GromitData *data,
				     GdkDevice  *dev,
				     gint        search_radius,
                                     GromitArrowType arrow_end,
                                     gint       *x0,
                                     gint       *y0,
				     gint       *ret_width,
				     gfloat     *ret_direction);
void coord_list_prepend (GromitData *data, GdkDevice* dev, gint x, gint y, gint width);
void coord_list_free (GromitData *data, GdkDevice* dev);


#endif
