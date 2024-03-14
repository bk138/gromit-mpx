/*
 * Gromit-MPX -- a program for painting on the screen
 *
 * Gromit Copyright (C) 2000 Simon Budig <Simon.Budig@unix-ag.org>
 *
 * Gromit-MPX Copyright (C) 2009,2010 Christian Beier <dontmind@freeshell.org>
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

#ifndef SMOOTH_PRIV_H
#define SMOOTH_PRIV_H

#include <glib.h>
#include "drawing.h"

// ----------------------------- helpers -----------------------------

extern inline float square(gfloat x);

// ------------------ coordinate-related functions -------------------

typedef struct {
  gfloat x;
  gfloat y;
} xy;

extern inline  xy get_xy_from_coord(GList *ptr);
extern inline void set_coord_from_xy(xy *point, GList *ptr);
extern inline xy xy_vec_from_coords(GList *start, GList *end);
extern inline xy xy_vec(xy *start, xy *end);
extern inline xy xy_add(xy *point, xy *translation);
extern inline gfloat xy_length(xy *vec);
extern inline gfloat coord_distance(GList *p1, GList *p2);
static gfloat distance_from_line(GList *p, GList *line_start, GList *line_end);
extern inline gfloat find_coord_vec_rotation(GList *start0, GList *end0,
                                      GList *start1, GList *end1);
extern inline gfloat find_xy_vec_rotation(xy *start0, xy *end0,
                                          xy *start1, xy *end1);
extern inline gfloat direction_of_coord_vector(GList *p1, GList *p2);
extern inline gfloat angle_deg_snap(gfloat angle, gfloat tolerance, gboolean *snapped);

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

extern inline void unity2D(trans2D *m);
extern inline void rotate2D(gfloat angle, trans2D *m);
extern inline void translate2D(gfloat dx, gfloat dy, trans2D *m);
extern inline void scale2D(gfloat kx, gfloat ky, trans2D *m);
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
extern inline gboolean section_is_ortho(Section *ptr);
extern inline gboolean section_is_vertical(Section *ptr);

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

#endif
