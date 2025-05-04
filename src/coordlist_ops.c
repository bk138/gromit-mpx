/*
 * Gromit-MPX -- a program for painting on the screen
 *
 * Gromit Copyright (C) 2000 Simon Budig <Simon.Budig@unix-ag.org>
 *
 * Gromit-MPX Copyright (C) 2009,2010 Christian Beier <dontmind@freeshell.org>
 * Copyright (C) 2024 Pascal Niklaus
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include <math.h>
#include <string.h>
#include <assert.h>
#include <glib.h>

#include "main.h"
#include "drawing.h"
#include "coordlist_ops.h"

// ================== forward definitions and types ==================

// ------------------ coordinate-related functions -------------------

typedef struct {
  gfloat x;
  gfloat y;
} xy;

static xy get_xy_from_coord(GList *ptr);
static void set_coord_from_xy(xy *point, GList *ptr);
static xy xy_vec_from_coords(GList *start, GList *end);
static xy xy_vec(xy *start, xy *end);
static xy xy_add(xy *point, xy *translation);
static gfloat xy_length(xy *vec);
static gfloat coord_distance(GList *p1, GList *p2);
static gfloat distance_from_line(GList *p, GList *line_start, GList *line_end);
static gfloat find_coord_vec_rotation(GList *start0, GList *end0,
                                      GList *start1, GList *end1);
static gfloat find_xy_vec_rotation(xy *start0, xy *end0,
                                   xy *start1, xy *end1);
static gfloat direction_of_coord_vector(GList *p1, GList *p2);
static gfloat angle_deg_snap(gfloat angle, gfloat tolerance, gboolean *snapped);

// -------------------- 2D affine transformations --------------------

typedef struct {
  union {
    gfloat m[6];
    struct {
      gfloat m_11; gfloat m_12; gfloat m_13;
      gfloat m_21; gfloat m_22; gfloat m_23;
    };
  };
} trans2D;

static void unity2D(trans2D *m);
static void rotate2D(gfloat angle, trans2D *m);
static void translate2D(gfloat dx, gfloat dy, trans2D *m);
static void scale2D(gfloat kx, gfloat ky, trans2D *m);
static void add2D(trans2D *t, trans2D *m);
static xy apply2D_xy(xy *coord, trans2D *m);
static void apply2D_coords(GList *start, GList *end, trans2D *m);

// ------------------------ sections-related -------------------------

/*
 * Section of a path in GList with 'GromitStrokecoordinate's
 *
 * 'start' and 'end' point into GList if 'GromitStrokecooordinate's
 *
 * 'xy_start' and 'xy_end' are the coordinates of the first and last
 * point; these are intermediate values and not necessarily in sync
 * with the GList pointed into.
 *
 * 'direction' is the orthogonal direction in degrees (0,90,180,270),
 * or -999 indicating a non-orthogonal section.
 */
#define NON_ORTHO_ANGLE -999

typedef struct {
  GList *start;
  GList *end;
  xy xy_start;
  xy xy_end;
  gint direction;
} Section;

static xy section_center(Section *ptr);
static xy section_dxdy(Section *ptr);
static gfloat section_diag_len(Section *ptr);
static gboolean section_is_ortho(Section *ptr);
static gboolean section_is_vertical(Section *ptr);

static GList * build_section_list(GList *coords, gint max_angular_deviation, gint min_ortho_len);

// ----------- stuff for douglas-peucker path simplication -----------

/*
 * a range of a GList of 'GromitStrokeCoordinate's, ranging from
 * 'first' up to and including 'last'
 */
typedef struct {
    GList *first;
    GList *last;
} ListRange;

static GList *push_list_range(GList *list, GList *first, GList *last);
static GList *pop_list_range(GList *list, GList **first, GList **last);

// ----------------- stuff for catmull-rom smoothing -----------------

static void cr_f_mul_grxy(gfloat f, GList *p1, GList *p2, xy *xy);
static void cr_f_mul_xy(gfloat f, xy *p1, xy *p2, xy *xy);
static gfloat catmull_rom_tj(gfloat ti,
                             GromitStrokeCoordinate *pi,
                             GromitStrokeCoordinate *pj);

// ===================== end forward definitions =====================

// ----------------------------- helpers -----------------------------

inline static gfloat square(gfloat x) {
    return (x * x);
}

// ------------------ coordinate-related functions -------------------
//
// In function names, 'xy' refers to the float 'xy' type with just the
// two coordinates, while 'coord' refers to 'GList' elements holding a
// 'GromitStrokeCoordinate'.  'xy' is returned by value -- for simple
// functions, the compiler will likely  the call anyway.

static xy get_xy_from_coord(GList *ptr) {
    xy result;
    result.x = ((GromitStrokeCoordinate *)ptr->data)->x;
    result.y = ((GromitStrokeCoordinate *)ptr->data)->y;
    return result;
}

static void set_coord_from_xy(xy *point, GList *ptr) {
    GromitStrokeCoordinate *coord = ptr->data;
    coord->x = point->x + 0.5;
    coord->y = point->y + 0.5;
}

static xy xy_vec_from_coords(GList *start, GList *end) {
    xy result;
    result.x = ((GromitStrokeCoordinate *)end->data)->x -
               ((GromitStrokeCoordinate *)start->data)->x;
    result.y = ((GromitStrokeCoordinate *)end->data)->y -
               ((GromitStrokeCoordinate *)start->data)->y;
    return result;
}

static xy xy_vec(xy *start, xy *end) {
    xy result;
    result.x = end->x - start->x;
    result.y = end->y - start->y;
    return result;
}

static xy xy_add(xy *point, xy *translation) {
    xy result = *point;
    result.x += translation->x;
    result.y += translation->y;
    return result;
}

static  gfloat xy_length(xy *vec) {
    return sqrtf(vec->x * vec->x + vec->y * vec->y);
}

 gfloat coord_distance(GList *p1, GList *p2) {
    GromitStrokeCoordinate *g1 = p1->data;
    GromitStrokeCoordinate *g2 = p2->data;
    gfloat dx = g1->x - g2->x;
    gfloat dy = g1->y - g2->y;
    return sqrtf(dx * dx + dy * dy);
}

/*
 * distance of point 'p' from line a line defined by points
 * 'line_start' and 'line_end'
 */
static gfloat distance_from_line(GList *p,
                                 GList *line_start,
                                 GList *line_end) {
  GromitStrokeCoordinate * gp = p->data;
  GromitStrokeCoordinate * gline_start = line_start->data;
  GromitStrokeCoordinate * gline_end = line_end->data;

    gfloat a = (gline_start->y - gline_end->y) * gp->x +
               (gline_end->x - gline_start->x) * gp->y +
               (gline_start->x * gline_end->y - gline_end->x * gline_start->y);
    gfloat l = sqrt(square(gline_start->x - gline_end->x) +
                    square(gline_start->y - gline_end->y));
    return fabs(a / l);
}

/*
 * find rotation that turns vector0 into direction of vector1.  All
 * coordinates are pointers into element of a GList of
 * 'GromitStrokeCooordinate's
 */
static  gfloat find_coord_vec_rotation(GList *start0,
                                       GList *end0,
                                       GList *start1,
                                       GList *end1) {
    xy v1 = xy_vec_from_coords(start0, end0);
    xy v2 = xy_vec_from_coords(start1, end1);
    return atan2(v1.x * v2.y - v2.x * v1.y, v1.x * v2.x + v1.y * v2.y);
}

/*
 * find rotation that turns vector0 into direction of vector1.
 */
static gfloat find_xy_vec_rotation(xy *start0,
                                   xy *end0,
                                   xy *start1,
                                   xy *end1) {
    xy v1 = xy_vec(start0, end0);
    xy v2 = xy_vec(start1, end1);
    return atan2(v1.x * v2.y - v2.x * v1.y, v1.x * v2.x + v1.y * v2.y);
}

/*
 * get angle (direction) of vector (p1->p2)
 */
static gfloat direction_of_coord_vector(GList *p1, GList *p2) {
    assert(p1 && p2);
    xy xy1 = get_xy_from_coord(p1);
    xy xy2 = get_xy_from_coord(p2);
    return atan2(xy2.y - xy1.y, xy2.x - xy1.x);
}

/*
 * check if angle is close to horizontal or vertical, within
 * tolerance, and if they are, "snap" angle to these
 */
static gfloat angle_deg_snap(gfloat angle,
                             gfloat tolerance,
                             gboolean *snapped) {
    if (angle < 0) angle += 360.0;
    if (angle == 360.0) angle = 0.0;
    gfloat ortho = round(angle / 90.0) * 90.0;
    if (fabs(angle - ortho) < tolerance) {
        if (snapped) *snapped = TRUE;
        angle = remainderf(ortho, 360.0);
        if (angle < 0) angle += 360.0;
    } else {
        if (snapped) *snapped = FALSE;
    }
    return angle;
}

// -------------------- 2D affine transformations --------------------

static void unity2D(trans2D *m) {
    memcpy(m->m, ((gfloat[6]){1.0, 0.0, 0.0, 0.0, 1.0, 0.0}), sizeof(gfloat[6]));
}

static void rotate2D(gfloat angle, trans2D *m) {
    unity2D(m);
    m->m_11 = m->m_22 = cos(angle);
    m->m_12 = sin(angle);
    m->m_21 = -m->m_12;
}

static void translate2D(gfloat dx, gfloat dy, trans2D *m) {
    unity2D(m);
    m->m_13 = dx;
    m->m_23 = dy;
}

static void scale2D(gfloat kx, gfloat ky, trans2D *m) {
    unity2D(m);
    m->m_11 = kx;
    m->m_22 = ky;
}

static void add2D(trans2D *t, trans2D *m) {
    trans2D new;
    new.m_11 = t->m_11 * m->m_11 + t->m_12 * m->m_21;
    new.m_12 = t->m_11 * m->m_12 + t->m_12 * m->m_22;
    new.m_13 = t->m_11 * m->m_13 + t->m_12 * m->m_23 + t->m_13;
    new.m_21 = t->m_21 * m->m_11 + t->m_22 * m->m_21;
    new.m_22 = t->m_21 * m->m_12 + t->m_22 * m->m_22;
    new.m_23 = t->m_21 * m->m_13 + t->m_22 * m->m_23 + t->m_23;
    *m = new;
}

/*
 * apply a 2d transformation to xy coordinate
 */
static xy apply2D_xy(xy *coord, trans2D *m) {
    xy result;
    result.x = m->m_11 * coord->x + m->m_12 * coord->y + m->m_13;
    result.y = m->m_21 * coord->x + m->m_22 * coord->y + m->m_23;
    return result;
}

/*
 * apply a 2d transformation to section of a 'GList' of
 * 'Gromitstrokecoordinate's
 */
static void apply2D_coords(GList *start, GList *end, trans2D *m) {
    while (start) {
        GromitStrokeCoordinate *ptr = start->data;
        gfloat newx = m->m_11 * ptr->x + m->m_12 * ptr->y + m->m_13;
        ptr->y = m->m_21 * ptr->x + m->m_22 * ptr->y + m->m_23 + 0.5;
        ptr->x = newx + 0.5;
        if (start == end) break;
        start = start->next;
    }
}

// ---------------------- path-section-related -----------------------

/*
 * get middle point between start and end of section by accessing
 * original stroke coordinates
 */
static xy section_center(Section *ptr) {
    xy start = get_xy_from_coord(ptr->start);
    xy end = get_xy_from_coord(ptr->end);
    start.x = 0.5 * (start.x + end.x);
    start.y = 0.5 * (start.y + end.y);
    return start;
}

/*
 * get vector from start to end of section by accessing original
 * stroke coordinates
 */
static xy section_dxdy(Section *ptr) {
    xy start = get_xy_from_coord(ptr->start);
    xy end = get_xy_from_coord(ptr->end);
    start.x = (end.x - start.x);
    start.y = (end.y - start.y);
    return start;
}

/*
 * get distance between start and end point of section
 */
static gfloat section_diag_len(Section *ptr) {
    xy start = get_xy_from_coord(ptr->start);
    xy end = get_xy_from_coord(ptr->end);
    return sqrt(square(start.x - end.x) + square(start.y - end.y));
}

/*
 * is section horizontal or vertical ?
 */
static gboolean section_is_ortho(Section *ptr) {
    return (ptr->direction % 90) == 0;
}

/*
 * is section vertical ?
 */
static gboolean section_is_vertical(Section *ptr) {
    return (ptr->direction % 180 == 90);
}

/*
 * scan GList of 'GromitStrokeCoordinate's and build list with
 * 'Sections' that are orthogonal (withing +- max_angular_deviation) or
 * 'free'
 */
static GList *build_section_list(GList *const coords,
                                 const gint max_angular_deviation,
                                 const gint min_ortho_len) {
    GList *section_list = NULL;
    GList *coords_ptr = coords;

    while (coords_ptr && coords_ptr->next) {
        Section *new_section = g_malloc(sizeof(Section));
        new_section->start = new_section->end = coords_ptr;

        // check if section is orthogonal
        gboolean ortho;
        gint angle, angle0;
        while (new_section->end->next) {
            angle = direction_of_coord_vector(new_section->end, new_section->end->next) * 180 / M_PI;
            angle = angle_deg_snap(angle, max_angular_deviation, &ortho);
            if (!ortho)
                break;
            if (new_section->start == new_section->end)
                angle0 = angle;
            else if (angle != angle0)
                break;
            new_section->end = new_section->end->next;
        }

        // if section exceeds minimum length, add orthogonal section to list
        if (section_diag_len(new_section) >= min_ortho_len) {
            new_section->direction = angle0;
            section_list = g_list_append(section_list, new_section);
            // keep only first and last coordinate in section
            GList *const ptr = new_section->start;
            while (ptr->next != new_section->end) {
                g_free(ptr->next->data);
                GList *tmp = g_list_delete_link(coords, ptr->next);
                assert(tmp == coords);
            }
            coords_ptr = new_section->end;
            continue;
        }

        // if not, include it in free (non-orthogonal) section
        while (new_section->end->next) {
          angle = direction_of_coord_vector(new_section->end, new_section->end->next) * 180 / M_PI;
          angle = angle_deg_snap(angle, max_angular_deviation, &ortho);
          if (ortho) break;
          new_section->end = new_section->end->next;
        }

        new_section->direction = NON_ORTHO_ANGLE;
        section_list = g_list_append(section_list, new_section);
        coords_ptr = new_section->end;
    }

    // cleanup: join successive non-orthogonal sections
    GList *ptr = section_list;
    while (ptr && ptr->next) {
        gint dir = ((Section *)ptr->data)->direction;
        gint nextdir = ((Section *)ptr->next->data)->direction;
        if (dir == nextdir) {
            ((Section *)ptr->data)->end = ((Section *)ptr->next->data)->end;
            g_free(ptr->next->data);
            section_list = g_list_delete_link(section_list, ptr->next);
        } else {
            ptr = ptr->next;
        }
    }

    // fill in start and end coordinates
    for (ptr = section_list; ptr; ptr = ptr->next) {
        Section *sec = ptr->data;
        sec->xy_start = get_xy_from_coord(sec->start);
        sec->xy_end = get_xy_from_coord(sec->end);
    }

    return section_list;
}

void coord_list_prepend (GromitData *data, 
			 GdkDevice* dev, 
			 gint x, 
			 gint y, 
			 gint width)
{
  /* get the data for this device */
  GromitDeviceData *devdata = g_hash_table_lookup(data->devdatatable, dev);

  GromitStrokeCoordinate *point;

  point = g_malloc (sizeof (GromitStrokeCoordinate));
  point->x = x;
  point->y = y;
  point->width = width;

  devdata->coordlist = g_list_prepend (devdata->coordlist, point);
}


void coord_list_free (GromitData *data, 
		      GdkDevice* dev)
{
  /* get the data for this device */
  GromitDeviceData *devdata = g_hash_table_lookup(data->devdatatable, dev);

  GList *ptr;
  ptr = devdata->coordlist;

  while (ptr)
    {
      g_free (ptr->data);
      ptr = ptr->next;
    }

  g_list_free (devdata->coordlist);

  devdata->coordlist = NULL;
}

/*
 * for double-ended arrows, two separate calls are required
 */

gboolean coord_list_get_arrow_param (GromitData      *data,
				     GdkDevice       *dev,
				     gint            search_radius,
                                     GromitArrowType arrow_end,
                                     gint            *x0,
                                     gint            *y0,
				     gint            *ret_width,
				     gfloat          *ret_direction)
{
  gint r2, dist;
  gboolean success = FALSE;
  GromitStrokeCoordinate  *cur_point, *valid_point;
  /* get the data for this device */
  GromitDeviceData *devdata = g_hash_table_lookup(data->devdatatable, dev);
  GList *ptr = devdata->coordlist;
  gfloat width;

  valid_point = NULL;

  if (ptr)
    {
      if (arrow_end == GROMIT_ARROW_START)
          ptr = g_list_last(ptr);
      cur_point = ptr->data;
      *x0 = cur_point->x;
      *y0 = cur_point->y;
      r2 = search_radius * search_radius;
      dist = 0;

      while (ptr && dist < r2)
        {
          if (arrow_end == GROMIT_ARROW_END)
            ptr = ptr->next;
          else
            ptr = ptr->prev;
          if (ptr)
            {
              cur_point = ptr->data;
              dist = (cur_point->x - *x0) * (cur_point->x - *x0) +
                     (cur_point->y - *y0) * (cur_point->y - *y0);
              width = cur_point->width * devdata->cur_context->arrowsize;
              if (width * 2 <= dist &&
                  (!valid_point || valid_point->width < cur_point->width))
                valid_point = cur_point;
            }
        }

      if (valid_point)
        {
          *ret_width = MAX (valid_point->width * devdata->cur_context->arrowsize,
                            2);
          *ret_direction = atan2 (*y0 - valid_point->y, *x0 - valid_point->x);
          success = TRUE;
        }
    }

  return success;
}


// ----------------------- orthogonalize path ------------------------

void orthogonalize(GList *const coords,
                   const gint max_angular_deviation,
                   const gint min_ortho_len) {
    GList *sec_list =
        build_section_list(coords, max_angular_deviation, min_ortho_len);

    if (g_list_length(sec_list) > 1) {
        // determine "fixed" coordinate of H and V sections (x for V and y
        // for H) and adjust  start and end points of path accordingly
        for (GList *ptr = sec_list; ptr; ptr = ptr->next) {
            Section *const sec = ((Section *)ptr->data);
            if (section_is_ortho(sec)) {
                xy center;
                if (!ptr->prev)
                    center = sec->xy_start;
                else if (!ptr->next)
                    center = sec->xy_end;
                else
                    center = section_center(sec);
                gboolean horiz = (sec->direction == 0 || sec->direction == 180);
                if (horiz) {
                    sec->xy_start.y = sec->xy_end.y = center.y;
                } else {
                    sec->xy_start.x = sec->xy_end.x = center.x;
                }
            }
        }

        // now "join" ends of adjacent sections
        for (GList *ptr = sec_list; ptr; ptr = ptr->next) {
            Section *const sec = ptr->data;
            if (section_is_ortho(sec)) {
                if (ptr->next && section_is_ortho(ptr->next->data)) {
                    Section *const next_sec = ptr->next->data;
                    // join orthogonal section to other orthogonal section
                    const gboolean first_v = section_is_vertical(sec);
                    const gboolean second_v = section_is_vertical(next_sec);
                    if (first_v != second_v) {
                        // sections meet at 90 degrees -> join them
                        if (first_v) {
                            sec->xy_end.y = next_sec->xy_start.y;
                            next_sec->xy_start.x = sec->xy_end.x;
                        } else {
                            sec->xy_end.x = next_sec->xy_start.x;
                            next_sec->xy_start.y = sec->xy_end.y;
                        }
                    } else {
                        // sections are parallel -> align ends
                        if (first_v) {
                            sec->xy_end.y = next_sec->xy_start.y =
                                0.5 * (sec->xy_end.y + next_sec->xy_start.y);
                        } else {
                            sec->xy_end.x = next_sec->xy_start.x =
                                0.5 * (sec->xy_end.x + next_sec->xy_start.x);
                        }
                    }
                } else {
                    if (ptr->prev) {
                        Section *const prev_sec = ((Section *)ptr->prev->data);
                        if (section_is_ortho(prev_sec)) {
                            sec->xy_start = prev_sec->xy_end;
                        }
                    }
                }
            } else {
                // section is not orthogonal
                trans2D m, t;
                if (ptr->prev && ptr->next) {
                    Section *const next_sec = ((Section *)ptr->next->data);
                    Section *const prev_sec = ((Section *)ptr->prev->data);
                    gboolean next_vert = section_is_vertical(next_sec);
                    gboolean prev_vert = section_is_vertical(prev_sec);

                    // one could fine-tune the orientation of the free section,
                    // based on how skewed the adjacent orthogonal sections are.
                    // gfloat angle = 0.5 * (remainderf(section_angle(prev_sec), M_PI_2) +
                    //                       remainderf(section_angle(next_sec), M_PI_2));
                    // rotate2D(angle, &m);  // or -angle ??
                    // apply2D(sec->start, sec->end, &m);

                    xy delta = section_dxdy(sec);

                    if (prev_vert != next_vert) {
                        // prev & next sections are at 90 degrees ->
                        // adjust their lengths to accomodate intermediate section.
                        //
                        // TODO: this could produce artifacts when ends would be moved back
                        // too much; then, the section could be scaled down to fit or rotated.
                        // But this does not seem to happen...
                        if (prev_vert) {
                            next_sec->xy_start.x = prev_sec->xy_end.x + delta.x;
                            prev_sec->xy_end.y = next_sec->xy_start.y - delta.y;
                        } else {
                            next_sec->xy_start.y = prev_sec->xy_end.y + delta.y;
                            prev_sec->xy_end.x = next_sec->xy_start.x - delta.x;
                        }
                        // update fields in Section
                        sec->xy_start = prev_sec->xy_end;
                        sec->xy_end = next_sec->xy_start;
                        // move section to right place
                        GromitStrokeCoordinate *point = (sec->start->data);
                        translate2D(prev_sec->xy_end.x - point->x,
                                    prev_sec->xy_end.y - point->y,
                                    &m);
                    } else {
                      // prev & next are somehow parallel -> fit free section between adjacent sections
                      gfloat x0 = prev_sec->xy_end.x;
                      gfloat y0 = prev_sec->xy_end.y;
                      translate2D(-x0, -y0, &m);
                      gfloat rot = find_xy_vec_rotation(&sec->xy_start, &sec->xy_end,
                                                        &prev_sec->xy_end, &next_sec->xy_start);
                      rotate2D(rot, &t);
                      add2D(&t, &m);
                      xy tmpvec = xy_vec(&prev_sec->xy_end, &next_sec->xy_start);
                      gfloat scale = xy_length(&tmpvec) / section_diag_len(sec);
                      scale2D(scale, scale, &t);
                      add2D(&t, &m);
                      translate2D(x0, y0, &t);
                      add2D(&t, &m);
                    }
                    apply2D_coords(sec->start, sec->end, &m);
                }
            }
        }

        // copy start and end points of orthogonal sections to coords
        for (GList *ptr = sec_list; ptr; ptr = ptr->next) {
            Section *const sec = ((Section *)ptr->data);
            if (section_is_ortho(sec)) {
                set_coord_from_xy(&sec->xy_start, sec->start);
                set_coord_from_xy(&sec->xy_end, sec->end);
            }
        }
    }

    g_list_free_full(sec_list, g_free);
}

/*
 * insert points into list of 'GromitStrokeCoordinate's so that the
 * distance between points becomes smaller than 'max_distance'
 */
void add_points(GList *coords, gfloat max_distance) {
    GList *ptr = coords;
    while (ptr && ptr->next) {
        gfloat d = coord_distance(ptr, ptr->next);
        if (d > max_distance) {
            gint n_sections = ceilf(d / max_distance);
            GromitStrokeCoordinate *const p0 = ptr->data;
            GromitStrokeCoordinate *const p1 = ptr->next->data;

            for (gint step = n_sections - 1; step > 0; step--) {
                gfloat k = (gfloat)step / n_sections;

                GromitStrokeCoordinate *new_coord =
                    g_malloc(sizeof(GromitStrokeCoordinate));
                new_coord->width = p0->width;
                new_coord->x = p0->x + (p1->x - p0->x) * k + 0.5;
                new_coord->y = p0->y + (p1->y - p0->y) * k + 0.5;
                GList *tmp = g_list_insert_before(coords, ptr->next, new_coord);
                assert(tmp == coords);
            }
        }
        ptr = ptr->next;
    }
}

/*
 * add rounded corners between sections
 */
void round_corners(GList *coords, gint radius, gint steps, gboolean circular) {
    GList *ptr = coords;
    if (ptr && g_list_length(ptr) > 2) {
        gfloat prev_len, next_len;
        const gint width = ((GromitStrokeCoordinate *)ptr->data)->width;
        for (;;) {
            gboolean is_last = (ptr->next == NULL);
            GList *next_pt = is_last ? coords->next : ptr->next;
            if (circular || !is_last) {
                prev_len = next_len;
                next_len = coord_distance(ptr, next_pt);
                if (ptr != coords && next_len > 2 * radius && prev_len > 2 * radius) {
                    const gfloat rot =
                        find_coord_vec_rotation(ptr->prev, ptr, ptr, next_pt);
                    const gfloat beta = rot / steps;
                    const gfloat a = 2 * radius * tan((M_PI - rot) / 2) * sin(rot / (2 * steps));

                    xy vec = xy_vec_from_coords(ptr->prev, ptr);
                    // move back point by radius
                    xy point = get_xy_from_coord(ptr);
                    gfloat k = radius / xy_length(&vec);
                    point.x -= (vec.x * k);
                    point.y -= (vec.y * k);
                    set_coord_from_xy(&point, ptr);

                    // initial step with length a
                    k *= (a / radius);
                    vec.x *= k;
                    vec.y *= k;
                    trans2D m;
                    rotate2D(beta / 2.0, &m);

                    vec = apply2D_xy(&vec, &m);
                    rotate2D(-beta, &m);

                    for (gint i = 0; i < steps; i++) {
                        vec = apply2D_xy(&vec, &m);
                        point = xy_add(&point, &vec);
                        GromitStrokeCoordinate *new_coord = g_malloc(sizeof(GromitStrokeCoordinate));
                        GList *tmp = g_list_insert_before(coords, ptr->next, new_coord);
                        assert(tmp == coords);
                        new_coord->width = width;
                        ptr = ptr->next;
                        set_coord_from_xy(&point, ptr);
                    }
                    if (is_last && circular) {
                        set_coord_from_xy(&point, coords);
                    }
                }
            }
            if (is_last)
                break;
            ptr = ptr->next;
        }
    }
}

/*
 * join ends of coordinates if their distance does not exceed max_distance
 * and if the initial segments are long enough
 */
gboolean snap_ends(GList *coords, gint max_distance) {
    if (g_list_length(coords) < 3) return FALSE;
    GList *last = g_list_last(coords);
    gfloat start_end_dist = coord_distance(coords, last);
    gfloat start_seg_len = coord_distance(coords, coords->next);
    gfloat end_seg_len = coord_distance(last, last->prev);

    if (start_end_dist <= max_distance &&
        start_seg_len > 0.7 * start_end_dist &&
        end_seg_len > 0.7 * start_end_dist) {
        xy p0 = get_xy_from_coord(coords);
        xy p1 = get_xy_from_coord(last);
        p0.x = 0.5 * (p0.x + p1.x);
        p0.y = 0.5 * (p0.y + p1.y);
        set_coord_from_xy(&p0, coords);
        set_coord_from_xy(&p0, last);
        return TRUE;
    }
    return FALSE;
}

// ----------------- douglas-peucker point reduction -----------------

/*
 * push a list range (first, last) to the front of a list
 */
static GList *push_list_range(GList *list, GList *first, GList *last) {
    ListRange *new_element = g_malloc(sizeof(ListRange));
    new_element->first = first;
    new_element->last = last;
    return g_list_prepend(list, new_element);
}

/*
 * pop a list range (first, last) from the front of a list
 */
static GList *pop_list_range(GList *list, GList **first, GList **last) {
    g_assert(list != NULL);
    *first = ((ListRange *)list->data)->first;
    *last = ((ListRange *)list->data)->last;
    GList *list_new = g_list_remove_link(list, list);
    g_free(list->data);
    g_list_free_1(list);
    return list_new;
}

/*
 * perform Douglas-Peucker smoothing of 'coords' with distance
 * threshold 'epsilon'. Based on
 * https://namekdev.net/2014/06/iterative-version-of-ramer-douglas-peucker-line-simplification-algorithm/
 */
void douglas_peucker(GList *coords, gfloat epsilon) {
    GList *first = coords;
    GList *last = g_list_last(coords);

    if (first != last) {
        GList *ranges = push_list_range(NULL, first, last);
        while (ranges != NULL) {
            ranges = pop_list_range(ranges, &first, &last);
            {
                int i = 1;
                GList *tmp = first;
                while (tmp != last) {
                    i++;
                    tmp = tmp->next;
                }
            }

            gfloat dmax = 0.0;
            GList *max_element = first;
            GList *ptr = first->next;
            if (ptr != last) {
                // d-p fails when start and end pts near-identical -> use dist from start instead
                gboolean too_close = coord_distance(first, last) < 10;
                while (ptr && ptr != last) {
                    gfloat d;
                    if (too_close) {
                        d = coord_distance(ptr, first);
                    } else {
                        d = distance_from_line(ptr, first, last);
                    }
                    if (d > dmax) {
                        dmax = d;
                        max_element = ptr;
                    }
                    ptr = ptr->next;
                }
                if (dmax > epsilon) {
                    ranges = push_list_range(ranges, first, max_element);
                    ranges = push_list_range(ranges, max_element, last);
                } else {
                    first->next->prev = NULL;
                    last->prev->next = NULL;
                    g_list_free_full(first->next, g_free);
                    first->next = last;
                    last->prev = first;
                }
            }
        }
    }
}

// -------------------  for catmull_rom_smoothing --------------------

/*
 * weighted mean of members of GList of 'GromitStrokeCoordinate's
 */
static void cr_f_mul_grxy(gfloat f, GList *p1, GList *p2, xy *xy) {
    xy->x = f * ((GromitStrokeCoordinate *)p1->data)->x +
            (1.0 - f) * ((GromitStrokeCoordinate *)p2->data)->x;
    xy->y = f * ((GromitStrokeCoordinate *)p1->data)->y +
            (1.0 - f) * ((GromitStrokeCoordinate *)p2->data)->y;
}

/*
 * weighted mean of members of (x,y) coordinates stored in xy
 */
static void cr_f_mul_xy(gfloat f, xy *p1, xy *p2, xy *xy) {
    xy->x = f * p1->x + (1.0 - f) * p2->x;
    xy->y = f * p1->y + (1.0 - f) * p2->y;
}
static gfloat catmull_rom_tj(gfloat ti,
                             GromitStrokeCoordinate *pi,
                             GromitStrokeCoordinate *pj) {
    gfloat dx = pj->x - pi->x;
    gfloat dy = pj->y - pi->y;
    return ti + sqrt(sqrt(dx * dx + dy * dy));
}

/*
 * centripetal Catmull-Rom interpolation with 'steps' steps between
 * coordinates.  Based on Python implementation at
 * https://en.wikipedia.org/wiki/Centripetal_Catmull%E2%80%93Rom_spline
 */
GList *catmull_rom(GList *coords, gint steps, gboolean circular) {
    gint segments = g_list_length(coords);

    if (segments < 3)  // at least 4 points needed
        return coords;

    GList *result = NULL;  // interpolated coordinated
    GList *srcptr = coords;
    gint width = ((GromitStrokeCoordinate *)coords->data)->width;

    GList *p0 = NULL;  // first segment: p0 = NULL
    GList *p1 = srcptr;
    GList *p2 = p1->next;
    GList *p3 = p2->next;  // last segment: p3 = NULL

    for (;;) {
        xy a1, a2, a3, b1, b2, pt;
        gfloat t0 = 0.0;
        gfloat t1 = 0.0;
        if (p0 == NULL) {
            if (circular) {
                t1 = catmull_rom_tj(t0, g_list_last(coords)->prev->data, p1->data);
            } else {
                t1 = 0.0;
            }
        } else {
            t1 = catmull_rom_tj(t0, p0->data, p1->data);
        }
        gfloat t2 = catmull_rom_tj(t1, p1->data, p2->data);
        gfloat t3 = t2;
        if (p3 == NULL) {
            if (circular) {
                t3 = catmull_rom_tj(t2, p2->data, coords->next->data);
            } else {
                t3 = t2;
            }
        } else {
            t3 = catmull_rom_tj(t2, p2->data, p3->data);
        }

        gfloat k = (t2 - t1) / steps;
        for (gint i = 0; i <= steps; i++) {
            gfloat t = t1 + k * i;

            if (p0 == NULL) {
              if (circular) {
                  cr_f_mul_grxy((t1 - t) / (t1 - t0), g_list_last(coords)->prev, p1, &a1);
              } else {
                  a1 = get_xy_from_coord(p1);
              }
            } else {
                cr_f_mul_grxy((t1 - t) / (t1 - t0), p0, p1, &a1);
            }
            cr_f_mul_grxy((t2 - t) / (t2 - t1), p1, p2, &a2);
            if (p3 == NULL) {
                if (circular) {
                    cr_f_mul_grxy((t3 - t) / (t3 - t2), p2, coords->next, &a3);
                } else {
                    a3 = get_xy_from_coord(p2);
                }
            } else {
                cr_f_mul_grxy((t3 - t) / (t3 - t2), p2, p3, &a3);
            }

            cr_f_mul_xy((t2 - t) / (t2 - t0), &a1, &a2, &b1);
            cr_f_mul_xy((t3 - t) / (t3 - t1), &a2, &a3, &b2);
            cr_f_mul_xy((t2 - t) / (t2 - t1), &b1, &b2, &pt);

            GromitStrokeCoordinate *coord = g_malloc(sizeof(GromitStrokeCoordinate));
            coord->width = width;
            coord->x = pt.x + 0.5;
            coord->y = pt.y + 0.5;

            result = g_list_append(result, coord);
        }
        p0 = p1;
        p1 = p1->next;
        p2 = p2->next;
        if (p3 == NULL)
            break;
        p3 = p3->next;
    }
    g_list_free_full(coords, g_free);
    return result;
}
