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

#ifndef CALLBACKS_H
#define CALLBACKS_H

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>


gboolean on_expose (GtkWidget *widget,
		    cairo_t* cr,
		    gpointer user_data);

gboolean on_configure (GtkWidget *widget,
		       GdkEventExpose *event,
		       gpointer user_data);


void on_screen_changed(GtkWidget *widget,
		       GdkScreen *previous_screen,
		       gpointer   user_data);

void on_monitors_changed(GdkScreen *screen,
			 gpointer   user_data);

void on_composited_changed(GdkScreen *screen,
			   gpointer   user_data);

void on_clientapp_selection_get (GtkWidget          *widget,
				 GtkSelectionData   *selection_data,
				 guint               info,
				 guint               time,
				 gpointer            user_data);

void on_clientapp_selection_received (GtkWidget *widget,
				      GtkSelectionData *selection_data,
				      guint time,
				      gpointer user_data);

gboolean on_buttonpress (GtkWidget *win, GdkEventButton *ev, gpointer user_data);

gboolean on_motion (GtkWidget *win, GdkEventMotion *ev, gpointer user_data);

gboolean on_buttonrelease (GtkWidget *win, GdkEventButton *ev, gpointer user_data);

void on_mainapp_selection_get (GtkWidget          *widget,
			       GtkSelectionData   *selection_data,
			       guint               info,
			       guint               time,
			       gpointer            user_data);


void on_mainapp_selection_received (GtkWidget *widget,
				    GtkSelectionData *selection_data,
				    guint time,
				    gpointer user_data);


void on_device_removed (GdkDeviceManager *device_manager,
			GdkDevice        *device,
			gpointer          user_data);

void on_device_added (GdkDeviceManager *device_manager,
		      GdkDevice        *device,
		      gpointer          user_data);

void on_signal(int signum);

/*
  menu callbacks
 */
gboolean on_toggle_paint(GtkWidget *widget,
			 GdkEventButton  *ev,
			 gpointer   user_data);

void on_toggle_paint_all (GtkMenuItem *menuitem,
			  gpointer     user_data);

void on_clear (GtkMenuItem *menuitem,
	       gpointer     user_data);

void on_toggle_vis(GtkMenuItem *menuitem,
		   gpointer     user_data);

void on_thicker_lines(GtkMenuItem *menuitem,
		      gpointer     user_data);

void on_thinner_lines(GtkMenuItem *menuitem,
		      gpointer     user_data);

void on_opacity_bigger(GtkMenuItem *menuitem,
		       gpointer     user_data);

void on_opacity_lesser(GtkMenuItem *menuitem,
		       gpointer     user_data);

void on_undo(GtkMenuItem *menuitem,
	     gpointer     user_data);

void on_redo(GtkMenuItem *menuitem,
	     gpointer     user_data);

void on_about(GtkMenuItem *menuitem,
	      gpointer     user_data);

void on_intro(GtkMenuItem *menuitem,
	      gpointer user_data);

void on_support_liberapay(GtkMenuItem *menuitem,
			  gpointer user_data);

void on_support_patreon(GtkMenuItem *menuitem,
			gpointer user_data);

void on_support_paypal(GtkMenuItem *menuitem,
		       gpointer user_data);

#endif
