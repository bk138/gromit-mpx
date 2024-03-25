/*
 * Gromit-MPX -- a program for painting on the screen
 *
 * Gromit Copyright (C) 2000 Simon Budig <Simon.Budig@unix-ag.org>
 *
 * Gromit-MPX Copyright (C) 2009,2010 Christian Beier <dontmind@freeshell.org>
 * Copytight (C) 2024 Pascal Niklaus
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

#ifndef COORDLIST_OPS_H
#define COORDLIST_OPS_H

#include "main.h"

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
gboolean snap_ends(GList *coords, gint max_distance);
void orthogonalize(GList *coords, gint max_angular_deviation, gint min_ortho_len);
void add_points(GList *coords, gfloat max_distance);
void round_corners(GList *coords, gint radius, gint steps, gboolean circular);
void douglas_peucker(GList *coords, gfloat epsilon);
GList *catmull_rom(GList *coords, gint steps, gboolean circular);

#endif
