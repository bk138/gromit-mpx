
#include <math.h>
#include "drawing.h"
#include "main.h"

void draw_line (GromitData *data,
		GdkDevice *dev,
		gint x1, gint y1,
		gint x2, gint y2)
{
  GdkRectangle rect;
  GromitDeviceData *devdata = g_hash_table_lookup(data->devdatatable, dev);

  rect.x = MIN (x1,x2) - data->maxwidth / 2;
  rect.y = MIN (y1,y2) - data->maxwidth / 2;
  rect.width = ABS (x1-x2) + data->maxwidth;
  rect.height = ABS (y1-y2) + data->maxwidth;

  if(data->debug)
    g_printerr("DEBUG: draw line from %d %d to %d %d\n", x1, y1, x2, y2);

  if (devdata->cur_context->paint_ctx)
    {
      cairo_set_line_width(devdata->cur_context->paint_ctx, data->maxwidth);
      cairo_set_line_cap(devdata->cur_context->paint_ctx, CAIRO_LINE_CAP_ROUND);
      cairo_set_line_join(devdata->cur_context->paint_ctx, CAIRO_LINE_JOIN_ROUND);
 
      cairo_move_to(devdata->cur_context->paint_ctx, x1, y1);
      cairo_line_to(devdata->cur_context->paint_ctx, x2, y2);
      cairo_stroke(devdata->cur_context->paint_ctx);

      data->modified = 1;

      gdk_window_invalidate_rect(gtk_widget_get_window(data->win), &rect, 0); 
    }

  data->painted = 1;
}


void draw_arrow (GromitData *data, 
		 GdkDevice *dev,
		 gint x1, gint y1,
		 gint width,
		 gfloat direction)
{
  GdkRectangle rect;
  GdkPoint arrowhead [4];

  /* get the data for this device */
  GromitDeviceData *devdata = g_hash_table_lookup(data->devdatatable, dev);

  width = width / 2;

  /* I doubt that calculating the boundary box more exact is very useful */
  rect.x = x1 - 4 * width - 1;
  rect.y = y1 - 4 * width - 1;
  rect.width = 8 * width + 2;
  rect.height = 8 * width + 2;

  arrowhead [0].x = x1 + 4 * width * cos (direction);
  arrowhead [0].y = y1 + 4 * width * sin (direction);

  arrowhead [1].x = x1 - 3 * width * cos (direction)
                       + 3 * width * sin (direction);
  arrowhead [1].y = y1 - 3 * width * cos (direction)
                       - 3 * width * sin (direction);

  arrowhead [2].x = x1 - 2 * width * cos (direction);
  arrowhead [2].y = y1 - 2 * width * sin (direction);

  arrowhead [3].x = x1 - 3 * width * cos (direction)
                       - 3 * width * sin (direction);
  arrowhead [3].y = y1 + 3 * width * cos (direction)
                       - 3 * width * sin (direction);

  if (devdata->cur_context->paint_ctx)
    {
      cairo_set_line_width(devdata->cur_context->paint_ctx, 1);
      cairo_set_line_cap(devdata->cur_context->paint_ctx, CAIRO_LINE_CAP_ROUND);
      cairo_set_line_join(devdata->cur_context->paint_ctx, CAIRO_LINE_JOIN_ROUND);
 
      cairo_move_to(devdata->cur_context->paint_ctx, arrowhead[0].x, arrowhead[0].y);
      cairo_line_to(devdata->cur_context->paint_ctx, arrowhead[1].x, arrowhead[1].y);
      cairo_line_to(devdata->cur_context->paint_ctx, arrowhead[2].x, arrowhead[2].y);
      cairo_line_to(devdata->cur_context->paint_ctx, arrowhead[3].x, arrowhead[3].y);
      cairo_fill(devdata->cur_context->paint_ctx);

      gdk_cairo_set_source_rgba(devdata->cur_context->paint_ctx, data->black);

      cairo_move_to(devdata->cur_context->paint_ctx, arrowhead[0].x, arrowhead[0].y);
      cairo_line_to(devdata->cur_context->paint_ctx, arrowhead[1].x, arrowhead[1].y);
      cairo_line_to(devdata->cur_context->paint_ctx, arrowhead[2].x, arrowhead[2].y);
      cairo_line_to(devdata->cur_context->paint_ctx, arrowhead[3].x, arrowhead[3].y);
      cairo_line_to(devdata->cur_context->paint_ctx, arrowhead[0].x, arrowhead[0].y);
      cairo_stroke(devdata->cur_context->paint_ctx);

      gdk_cairo_set_source_rgba(devdata->cur_context->paint_ctx, devdata->cur_context->paint_color);
    
      data->modified = 1;

      gdk_window_invalidate_rect(gtk_widget_get_window(data->win), &rect, 0); 
    }

  data->painted = 1;
}

