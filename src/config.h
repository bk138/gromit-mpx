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

#ifndef CONFIG_H
#define CONFIG_H

#include "main.h"

/* fallback device name for config file */
#define DEFAULT_DEVICE_NAME "default"

/**
   Select and parse system or user .cfg file.
   Returns TRUE if something got parsed successfully, FALSE otherwise.
*/
gboolean parse_config (GromitData *data);
int parse_args (int argc, char **argv, GromitData *data);

/* fallback hot key, if not specified on command line or in config file */
#ifndef DEFAULT_HOTKEY
#define DEFAULT_HOTKEY "F9"
#define DEFAULT_HOTKEY_XFCE "Home"
#endif
#ifndef DEFAULT_UNDOKEY
#define DEFAULT_UNDOKEY "F8"
#define DEFAULT_UNDOKEY_XFCE "End"
#endif
#ifndef DEFAULT_OPACITY
#define DEFAULT_OPACITY 0.75
#endif

void read_keyfile(GromitData *data);

void write_keyfile(GromitData *data);

#endif
