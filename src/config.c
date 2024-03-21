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

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include "config.h"
#include "main.h"
#include "build-config.h"

#define KEY_DFLT_SHOW_INTRO_ON_STARTUP TRUE

#define KEYFILE_FLAGS G_KEY_FILE_KEEP_COMMENTS|G_KEY_FILE_KEEP_TRANSLATIONS

static gpointer HOTKEY_SYMBOL_VALUE  = (gpointer) 5;
static gpointer UNDOKEY_SYMBOL_VALUE = (gpointer) 6;

/*
 * Functions for parsing the Configuration-file
 */

static gboolean parse_name (GScanner *scanner, GromitLookupKey *key)
{
  GTokenType token;

  guint buttons = 0;
  guint modifier = 0;
  gulong keys = 0;
  guint len = 0;

  token = g_scanner_cur_token(scanner);

  if (token != G_TOKEN_STRING)
    {
      g_scanner_unexp_token (scanner, G_TOKEN_STRING, NULL,
                             NULL, NULL, "aborting", TRUE);
      return 0;
    }

  len = strlen (scanner->value.v_string);
  key->name = g_strndup (scanner->value.v_string, len);

  token = g_scanner_get_next_token (scanner);

  /*
   * Are there any options to limit the scope of the definition?
   */

  if (token == G_TOKEN_LEFT_BRACE)
    {
      g_scanner_set_scope (scanner, 1);
      scanner->config->int_2_float = 0;
      modifier = buttons = keys = 0;
      while ((token = g_scanner_get_next_token (scanner))
             != G_TOKEN_RIGHT_BRACE)
        {
          if (token == G_TOKEN_SYMBOL)
            {
              if ((intptr_t) scanner->value.v_symbol <= 10)
                 buttons |= 1 << ((intptr_t) scanner->value.v_symbol - 1);
              else
                 modifier |= 1 << ((intptr_t) scanner->value.v_symbol - 11);
            }
          else if (token == G_TOKEN_INT)
            {
              if (scanner->value.v_int <= 10 && scanner->value.v_int > 0)
                buttons |= 1 << (scanner->value.v_int - 1);
              else
                g_printerr ("Only Buttons 1-10 are supported!\n");
            }
          // lower case and upper case write the same bit
          else if (token >= GDK_KEY_a && token <= GDK_KEY_z)
            keys |= 1 << (token - GDK_KEY_a);
          else if (token >= GDK_KEY_A && token <= GDK_KEY_Z)
            keys |= 1 << (token - GDK_KEY_A);
          else
            {
              g_printerr ("skipped token\n");
            }
        }
      g_scanner_set_scope (scanner, 0);
      scanner->config->int_2_float = 1;
      token = g_scanner_get_next_token (scanner);
    }

  key->state.buttons = buttons;
  key->state.modifiers = modifier;
  key->state.keys = keys;

  return 1;
}

gboolean parse_config (GromitData *data)
{
  gboolean status = FALSE;
  GromitPaintContext *context=NULL;
  GromitPaintContext *context_template=NULL;
  GScanner *scanner;
  GTokenType token;
  gchar *filename;
  int file;

  gboolean parsed;

  GromitPaintType type;
  GdkRGBA *fg_color=NULL;
  guint width, arrowsize, minwidth, maxwidth;
  GromitArrowType arrowtype;

  /* try user config location */
  filename = g_strjoin (G_DIR_SEPARATOR_S,
                        g_get_user_config_dir(), "gromit-mpx.cfg", NULL);
  if ((file = open(filename, O_RDONLY)) < 0)
      g_print("Could not open user config %s: %s\n", filename, g_strerror (errno));
  else
      g_print("Using user config %s\n", filename);


  /* try global config file */
  if (file < 0) {
      g_free(filename);
      filename = g_strdup (SYSCONFDIR "/gromit-mpx/gromit-mpx.cfg");
      if ((file = open(filename, O_RDONLY)) < 0)
	  g_print("Could not open system config %s: %s\n", filename, g_strerror (errno));
      else
	  g_print("Using system config %s\n", filename);
  }

  /* was the last possibility, no use to go on */
  if (file < 0) {
      g_free(filename);
      GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(data->win),
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_WARNING,
						 GTK_BUTTONS_CLOSE,
						 _("No usable config file found, falling back to default tools."));
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      return FALSE;
  }

  scanner = g_scanner_new (NULL);
  scanner->input_name = filename;
  scanner->config->case_sensitive = 0;
  scanner->config->scan_octal = 0;
  scanner->config->identifier_2_string = 0;
  scanner->config->char_2_token = 1;
  scanner->config->numbers_2_int = 1;
  scanner->config->int_2_float = 1;

  g_scanner_scope_add_symbol (scanner, 0, "PEN",    (gpointer) GROMIT_PEN);
  g_scanner_scope_add_symbol (scanner, 0, "LINE",   (gpointer) GROMIT_LINE);
  g_scanner_scope_add_symbol (scanner, 0, "RECT",   (gpointer) GROMIT_RECT);
  g_scanner_scope_add_symbol (scanner, 0, "ERASER", (gpointer) GROMIT_ERASER);
  g_scanner_scope_add_symbol (scanner, 0, "RECOLOR",(gpointer) GROMIT_RECOLOR);
  g_scanner_scope_add_symbol (scanner, 0, "HOTKEY",            HOTKEY_SYMBOL_VALUE);
  g_scanner_scope_add_symbol (scanner, 0, "UNDOKEY",           UNDOKEY_SYMBOL_VALUE);

  g_scanner_scope_add_symbol (scanner, 1, "BUTTON1", (gpointer) 1);
  g_scanner_scope_add_symbol (scanner, 1, "BUTTON2", (gpointer) 2);
  g_scanner_scope_add_symbol (scanner, 1, "BUTTON3", (gpointer) 3);
  g_scanner_scope_add_symbol (scanner, 1, "BUTTON4", (gpointer) 4);
  g_scanner_scope_add_symbol (scanner, 1, "BUTTON5", (gpointer) 5);
  g_scanner_scope_add_symbol (scanner, 1, "BUTTON6", (gpointer) 6);
  g_scanner_scope_add_symbol (scanner, 1, "BUTTON7", (gpointer) 7);
  g_scanner_scope_add_symbol (scanner, 1, "BUTTON8", (gpointer) 8);
  g_scanner_scope_add_symbol (scanner, 1, "BUTTON9", (gpointer) 9);
  g_scanner_scope_add_symbol (scanner, 1, "BUTTON10", (gpointer) 10);
  g_scanner_scope_add_symbol (scanner, 1, "SHIFT",   (gpointer) 11);
  g_scanner_scope_add_symbol (scanner, 1, "CONTROL", (gpointer) 13);
  g_scanner_scope_add_symbol (scanner, 1, "META",    (gpointer) 14);
  g_scanner_scope_add_symbol (scanner, 1, "ALT",     (gpointer) 14);

  g_scanner_scope_add_symbol (scanner, 2, "size",      (gpointer) 1);
  g_scanner_scope_add_symbol (scanner, 2, "color",     (gpointer) 2);
  g_scanner_scope_add_symbol (scanner, 2, "arrowsize", (gpointer) 3);
  g_scanner_scope_add_symbol (scanner, 2, "arrowtype", (gpointer) 4);
  g_scanner_scope_add_symbol (scanner, 2, "minsize",   (gpointer) 5);
  g_scanner_scope_add_symbol (scanner, 2, "maxsize",   (gpointer) 6);

  g_scanner_set_scope (scanner, 0);
  scanner->config->scope_0_fallback = 0;

  g_scanner_input_file (scanner, file);

  token = g_scanner_get_next_token (scanner);
  while (token != G_TOKEN_EOF)
    {
      if (token == G_TOKEN_STRING)
        {
          /*
           * New tool definition
           */
          GromitLookupKey keyName = {};

          parsed = parse_name (scanner, &keyName);

	  if(!parsed)
	      goto cleanup;

          token = g_scanner_cur_token(scanner);

          if (token != G_TOKEN_EQUAL_SIGN)
            {
              g_scanner_unexp_token (scanner, G_TOKEN_EQUAL_SIGN, NULL,
                                     NULL, NULL, "aborting", TRUE);
              goto cleanup;
            }

          token = g_scanner_get_next_token (scanner);

          /* defaults */
          type = GROMIT_PEN;
          width = 7;
          arrowsize = 0;
          arrowtype = GROMIT_ARROW_END;
          minwidth = 1;
          maxwidth = G_MAXUINT;
          fg_color = data->red;

          if (token == G_TOKEN_SYMBOL)
            {
              type = (GromitPaintType) scanner->value.v_symbol;
              token = g_scanner_get_next_token (scanner);
            }
          else if (token == G_TOKEN_STRING)
            {
              GromitLookupKey keyCopy = {};
              parsed = parse_name (scanner, &keyCopy);
	      if(!parsed)
		  goto cleanup;
              token = g_scanner_cur_token(scanner);
              context_template = g_hash_table_lookup (data->tool_config, key2string(keyCopy));
              if (context_template)
                {
                  type = context_template->type;
                  width = context_template->width;
                  arrowsize = context_template->arrowsize;
                  arrowtype = context_template->arrow_type;
                  minwidth = context_template->minwidth;
		  maxwidth = context_template->maxwidth;
                  fg_color = context_template->paint_color;
                }
              else
                {
                  g_printerr ("WARNING: Unable to copy \"%s\": "
                              "not yet defined!\n", key2string(keyCopy));
                }
            }
          else
            {
              g_printerr ("Expected Tool-definition "
                          "or name of template tool\n");
              goto cleanup;
            }

          /* Are there any tool-options?
           */

          if (token == G_TOKEN_LEFT_PAREN)
            {
              GdkRGBA *color = NULL;
              g_scanner_set_scope (scanner, 2);
              scanner->config->int_2_float = 1;
              token = g_scanner_get_next_token (scanner);
              while (token != G_TOKEN_RIGHT_PAREN)
                {
                  if (token == G_TOKEN_SYMBOL)
                    {
                      if ((intptr_t) scanner->value.v_symbol == 1)
                        {
                          token = g_scanner_get_next_token (scanner);
                          if (token != G_TOKEN_EQUAL_SIGN)
                            {
                              g_printerr ("Missing \"=\"... aborting\n");
                              goto cleanup;
                            }
                          token = g_scanner_get_next_token (scanner);
                          if (token != G_TOKEN_FLOAT)
                            {
                              g_printerr ("Missing Size (float)... aborting\n");
                              goto cleanup;
                            }
                          width = (guint) (scanner->value.v_float + 0.5);
                        }
                      else if ((intptr_t) scanner->value.v_symbol == 2)
                        {
                          token = g_scanner_get_next_token (scanner);
                          if (token != G_TOKEN_EQUAL_SIGN)
                            {
                              g_printerr ("Missing \"=\"... aborting\n");
                              goto cleanup;
                            }
                          token = g_scanner_get_next_token (scanner);
                          if (token != G_TOKEN_STRING)
                            {
                              g_printerr ("Missing Color (string)... "
                                          "aborting\n");
                              goto cleanup;
                            }
                          color = g_malloc (sizeof (GdkRGBA));
                          if (gdk_rgba_parse (color, scanner->value.v_string))
                            {
			      fg_color = color;
                            }
                          else
                            {
                              g_printerr ("Unable to parse color. "
                                          "Keeping default.\n");
                              g_free (color);
                            }
                          color = NULL;
                        }
                      else if ((intptr_t) scanner->value.v_symbol == 3)
                        {
                          token = g_scanner_get_next_token (scanner);
                          if (token != G_TOKEN_EQUAL_SIGN)
                            {
                              g_printerr ("Missing \"=\"... aborting\n");
                              goto cleanup;
                            }
                          token = g_scanner_get_next_token (scanner);
                          if (token != G_TOKEN_FLOAT)
                            {
                              g_printerr ("Missing Arrowsize (float)... "
                                          "aborting\n");
                              goto cleanup;
                            }
                          arrowsize = scanner->value.v_float;
                        }
                      else if ((intptr_t) scanner->value.v_symbol == 4)
                        {
                          token = g_scanner_get_next_token (scanner);
                          if (token != G_TOKEN_EQUAL_SIGN)
                            {
                              g_printerr ("Missing \"=\"... aborting\n");
                              goto cleanup;
                            }
                          token = g_scanner_get_next_token (scanner);
                          if (token != G_TOKEN_STRING)
                            {
                              g_printerr ("Missing Arrowsize (string)... "
                                          "aborting\n");
                              goto cleanup;
                            }
                          if (! strcasecmp(scanner->value.v_string, "end"))
                            arrowtype = GROMIT_ARROW_END;
                          else if (! strcasecmp(scanner->value.v_string, "start"))
                            arrowtype = GROMIT_ARROW_START;
                          else if (! strcasecmp(scanner->value.v_string, "double"))
                            arrowtype = GROMIT_ARROW_DOUBLE;
                          else
                            {
                              g_printerr ("Arrow type must be \"start\", \"end\", or \"double\"... "
                                          "aborting\n");
                              goto cleanup;
                            }
                        }
                      else if ((intptr_t) scanner->value.v_symbol == 5)
                        {
                          token = g_scanner_get_next_token (scanner);
                          if (token != G_TOKEN_EQUAL_SIGN)
                            {
                              g_printerr ("Missing \"=\"... aborting\n");
                              goto cleanup;
                            }
                          token = g_scanner_get_next_token (scanner);
                          if (token != G_TOKEN_FLOAT)
                            {
                              g_printerr ("Missing Minsize (float)... "
                                          "aborting\n");
                              goto cleanup;
                            }
                          minwidth = scanner->value.v_float;
                        }
                      else if ((intptr_t) scanner->value.v_symbol == 6)
                        {
                          token = g_scanner_get_next_token (scanner);
                          if (token != G_TOKEN_EQUAL_SIGN)
                            {
                              g_printerr ("Missing \"=\"... aborting\n");
                              goto cleanup;
                            }
                          token = g_scanner_get_next_token (scanner);
                          if (token != G_TOKEN_FLOAT)
                            {
                              g_printerr ("Missing Maxsize (float)... "
                                          "aborting\n");
                              goto cleanup;
                            }
                          maxwidth = scanner->value.v_float;
                        }
		      else
                        {
                          g_printerr ("Unknown tool type?????\n");
                        }
                    }
                  else
                    {
                      g_printerr ("skipped token!!!\n");
                    }
                  token = g_scanner_get_next_token (scanner);
                }
              g_scanner_set_scope (scanner, 0);
              token = g_scanner_get_next_token (scanner);
            }

          /*
           * Finally we expect a semicolon
           */

          if (token != ';')
            {
              g_printerr ("Expected \";\"\n");
              goto cleanup;
            }

          context = paint_context_new (data, type, fg_color, width,
                                       arrowsize, arrowtype, minwidth, maxwidth);
          g_hash_table_insert (data->tool_config, key2string(keyName), context);
        }
      else if (token == G_TOKEN_SYMBOL &&
               (scanner->value.v_symbol == HOTKEY_SYMBOL_VALUE ||
                scanner->value.v_symbol == UNDOKEY_SYMBOL_VALUE))
        {
          /*
           * Hot key definition
           */

          gpointer key_type = scanner->value.v_symbol;
          token = g_scanner_get_next_token(scanner);

          if (token != G_TOKEN_EQUAL_SIGN)
            {
              g_scanner_unexp_token (scanner, G_TOKEN_EQUAL_SIGN, NULL,
                                     NULL, NULL, "aborting", TRUE);
              goto cleanup;
            }

          token = g_scanner_get_next_token (scanner);

          if (token != G_TOKEN_STRING)
            {
              g_scanner_unexp_token (scanner, G_TOKEN_STRING, NULL,
                                     NULL, NULL, "aborting", TRUE);
              goto cleanup;
            }

          if (key_type == HOTKEY_SYMBOL_VALUE)
            {
              data->hot_keyval = g_strdup(scanner->value.v_string);
            }
          else if (key_type == UNDOKEY_SYMBOL_VALUE)
            {
              data->undo_keyval = g_strdup(scanner->value.v_string);
            }

          token = g_scanner_get_next_token(scanner);

          if (token != ';')
            {
              g_printerr ("Expected \";\"\n");
              goto cleanup;
            }
        }
      else
        {
          g_printerr ("Expected name of Tool to define or Hot key definition\n");
          goto cleanup;
        }

      token = g_scanner_get_next_token (scanner);
    }

  status = TRUE;

 cleanup:

  if (!status) {
      /* purge incomplete tool config */
      GHashTableIter it;
      gpointer value;
      g_hash_table_iter_init (&it, data->tool_config);
      while (g_hash_table_iter_next (&it, NULL, &value))
	  paint_context_free(value);
      g_hash_table_remove_all(data->tool_config);

      /* alert user */
      GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(data->win),
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_WARNING,
						 GTK_BUTTONS_CLOSE,
						 _("Failed parsing config file %s, falling back to default tools."),
						 filename);
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
  }

  g_scanner_destroy (scanner);
  close (file);
  g_free (filename);

  return status;
}


int parse_args (int argc, char **argv, GromitData *data)
{
   gint      i;
   gchar    *arg;
   gboolean  wrong_arg = FALSE;
   gboolean  activate = FALSE;

   for (i=1; i < argc ; i++)
     {
       arg = argv[i];
       if (strcmp (arg, "-a") == 0 ||
           strcmp (arg, "--active") == 0)
         {
           activate = TRUE;
         }
       else if (strcmp (arg, "-d") == 0 ||
                strcmp (arg, "--debug") == 0)
         {
           data->debug = 1;
         }
       else if (strcmp (arg, "-k") == 0 ||
                strcmp (arg, "--key") == 0)
         {
           if (i+1 < argc)
             {
               data->hot_keyval = argv[i+1];
               data->hot_keycode = 0;
               i++;
             }
           else
             {
               g_printerr ("-k requires an Key-Name as argument\n");
               wrong_arg = TRUE;
             }
         }
       else if (strcmp (arg, "-K") == 0 ||
                strcmp (arg, "--keycode") == 0)
         {
           if (i+1 < argc && atoi (argv[i+1]) > 0)
             {
               data->hot_keyval = NULL;
               data->hot_keycode = atoi (argv[i+1]);
               i++;
             }
           else
             {
               g_printerr ("-K requires an keycode > 0 as argument\n");
               wrong_arg = TRUE;
             }
         }
      else if (strcmp (arg, "-o") == 0 ||
                strcmp (arg, "--opacity") == 0)
         {
           if (i+1 < argc && strtod (argv[i+1], NULL) >= 0.0 && strtod (argv[i+1], NULL) <= 1.0)
             {
               data->opacity = strtod (argv[i+1], NULL);
               g_printerr ("Opacity set to: %.2f\n", data->opacity);
               gtk_widget_set_opacity(data->win, data->opacity);
               i++;
             }
           else
             {
               g_printerr ("-o requires an opacity >=0 and <=1 as argument\n");
               wrong_arg = TRUE;
             }
         }
       else if (strcmp (arg, "-u") == 0 ||
                strcmp (arg, "--undo-key") == 0)
         {
           if (i+1 < argc)
             {
               data->undo_keyval = argv[i+1];
               data->undo_keycode = 0;
               i++;
             }
           else
             {
               g_printerr ("-u requires an Key-Name as argument\n");
               wrong_arg = TRUE;
             }
         }
       else if (strcmp (arg, "-U") == 0 ||
                strcmp (arg, "--undo-keycode") == 0)
         {
           if (i+1 < argc && atoi (argv[i+1]) > 0)
             {
               data->undo_keyval = NULL;
               data->undo_keycode = atoi (argv[i+1]);
               i++;
             }
           else
             {
               g_printerr ("-U requires an keycode > 0 as argument\n");
               wrong_arg = TRUE;
             }
         }
       else if (strcmp (arg, "-V") == 0 ||
		strcmp (arg, "--version") == 0)
         {
	     g_print("Gromit-MPX " PACKAGE_VERSION "\n");
	     exit(0);
         }
       else
         {
           g_printerr ("Unknown Option for Gromit-MPX startup: \"%s\"\n", arg);
           wrong_arg = TRUE;
         }

       if (wrong_arg)
         {
           g_printerr ("Please see the Gromit-MPX manpage for the correct usage\n");
           exit (1);
         }
     }

   return activate;
}


void read_keyfile(GromitData *data)
{
    gchar *filename = g_strjoin (G_DIR_SEPARATOR_S,
				 g_get_user_config_dir(), "gromit-mpx.ini", NULL);

    /*
      set defaults
    */
    data->show_intro_on_startup = KEY_DFLT_SHOW_INTRO_ON_STARTUP;

    /*
      read actual settings
    */
    GError *error = NULL;
    GKeyFile *key_file = g_key_file_new ();

    if (!g_key_file_load_from_file (key_file, filename, KEYFILE_FLAGS, &error)) {
	// treat file-not-found not as an error
	if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
	    g_warning ("Error loading key file: %s", error->message);
	g_error_free(error);
	goto cleanup;
    }

    data->show_intro_on_startup = g_key_file_get_boolean (key_file, "General", "ShowIntroOnStartup", &error);
    data->opacity = g_key_file_get_double (key_file, "Drawing", "Opacity", &error);
    // 0.0 on not-found, but anyway, also don't use 0.0 when user-set
    if(data->opacity == 0)
	data->opacity = DEFAULT_OPACITY;

 cleanup:
    g_free(filename);
    g_key_file_free(key_file);
}


void write_keyfile(GromitData *data)
{
    gchar *filename = g_strjoin (G_DIR_SEPARATOR_S,
				 g_get_user_config_dir(), "gromit-mpx.ini", NULL);

    GError *error = NULL;
    GKeyFile *key_file = g_key_file_new ();

    g_key_file_set_boolean (key_file, "General", "ShowIntroOnStartup", data->show_intro_on_startup);
    g_key_file_set_double (key_file, "Drawing", "Opacity", data->opacity);

    // if file exists but is read-only, bail out
    if (access(filename, F_OK) == 0 && access(filename, W_OK) != 0) {
	g_warning ("Not overwriting read-only key file");
	goto cleanup;
    }

    // Save as a file.
    if (!g_key_file_save_to_file (key_file, filename, &error)) {
	g_warning ("Error saving key file: %s", error->message);
	g_error_free(error);
	goto cleanup;
    }

 cleanup:
    g_free(filename);
    g_key_file_free(key_file);
}
