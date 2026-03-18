
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
		 gfloat width,
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


void draw_circle (GromitData *data,
                 GdkDevice *dev,
                 gint x, gint y,
                 gfloat radius)
{
  GdkRectangle rect;
  GromitDeviceData *devdata = g_hash_table_lookup(data->devdatatable, dev);

  /* Invalidation rectangle */
  rect.x = x - radius - data->maxwidth / 2;
  rect.y = y - radius - data->maxwidth / 2;
  rect.width = 2 * radius + data->maxwidth;
  rect.height = 2 * radius + data->maxwidth;

  if (devdata->cur_context->paint_ctx)
    {
      cairo_set_line_width(devdata->cur_context->paint_ctx, data->maxwidth);
      cairo_set_line_cap(devdata->cur_context->paint_ctx, CAIRO_LINE_CAP_ROUND);
      cairo_set_line_join(devdata->cur_context->paint_ctx, CAIRO_LINE_JOIN_ROUND);

      if (devdata->cur_context->fill_color)
        {
          gdk_cairo_set_source_rgba(devdata->cur_context->paint_ctx, devdata->cur_context->fill_color);

          cairo_arc(devdata->cur_context->paint_ctx, x, y, radius, 0, 2 * M_PI);
          cairo_fill(devdata->cur_context->paint_ctx);

          gdk_cairo_set_source_rgba(devdata->cur_context->paint_ctx, devdata->cur_context->paint_color);
        }

      cairo_arc(devdata->cur_context->paint_ctx, x, y, radius, 0, 2 * M_PI);
      cairo_stroke(devdata->cur_context->paint_ctx);

      data->modified = 1;

      gdk_window_invalidate_rect(gtk_widget_get_window(data->win), &rect, 0);
    }

  data->painted = 1;
}

void draw_length_label (GromitData *data,
                        GdkDevice *dev,
                        gint x1, gint y1,
                        gint x2, gint y2)
{
  gdouble dx = x2 - x1;
  gdouble dy = y2 - y1;
  gdouble length = sqrt(dx * dx + dy * dy);

  if (length == 0) return;

  char label[64];
  snprintf(label, sizeof(label), "%.0f px", length);

  gdouble mx = (x1 + x2) / 2;
  gdouble my = (y1 + y2) / 2;

  draw_string_label(data, dev, mx, my, label);
}

void draw_string_label (GromitData *data, GdkDevice *dev, gint x, gint y, char *label)
{
  GromitDeviceData *devdata = g_hash_table_lookup(data->devdatatable, dev);
  cairo_t *cr = devdata->cur_context->paint_ctx;

  cairo_save(cr);

  cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, devdata->cur_context->textsize);
  cairo_text_extents_t extents;
  cairo_text_extents(cr, label, &extents);

  gdouble padding = 4.0;
  gdouble tx = x - extents.width / 2.0;
  gdouble ty = y - extents.height / 2.0;

  cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
  cairo_set_source_rgba(cr, 0, 0, 0, 0.5);
  cairo_rectangle(cr,
                  tx + extents.x_bearing - padding,
                  ty + extents.y_bearing - padding,
                  extents.width + 2 * padding,
                  extents.height + 2 * padding);
  cairo_fill(cr);

  cairo_set_source_rgba(cr, 1, 1, 1, 1.0);
  cairo_move_to(cr, tx, ty);
  cairo_show_text(cr, label);

  cairo_restore(cr);
  gdk_cairo_set_source_rgba(cr, devdata->cur_context->paint_color);

  /* Invalidation rectangle */
  GdkRectangle rect;
  rect.x = (int)(tx + extents.x_bearing - padding - 1);
  rect.y = (int)(ty + extents.y_bearing - padding - 1);
  rect.width = (int)(extents.width + 2 * padding + 3);
  rect.height = (int)(extents.height + 2 * padding + 3);

  data->modified = 1;

  gdk_window_invalidate_rect(gtk_widget_get_window(data->win), &rect, 0);
}