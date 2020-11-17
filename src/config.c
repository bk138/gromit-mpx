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
#include "gromit-mpx.h"
#include "build-config.h"

#define KEY_DFLT_SHOW_INTRO_ON_STARTUP TRUE

#define KEYFILE_FLAGS G_KEY_FILE_KEEP_COMMENTS|G_KEY_FILE_KEEP_TRANSLATIONS

static gpointer HOTKEY_SYMBOL_VALUE  = (gpointer) 3;
static gpointer UNDOKEY_SYMBOL_VALUE = (gpointer) 4;

/*
 * Functions for parsing the Configuration-file
 */

static gchar* parse_name (GScanner *scanner)
{
  GTokenType token;

  guint buttons = 0;
  guint modifier = 0;
  guint len = 0;
  gchar *name;

  token = g_scanner_cur_token(scanner);

  if (token != G_TOKEN_STRING)
    {
      g_scanner_unexp_token (scanner, G_TOKEN_STRING, NULL,
                             NULL, NULL, "aborting", TRUE);
      exit (1);
    }

  len = strlen (scanner->value.v_string);
  name = g_strndup (scanner->value.v_string, len + 3);

  token = g_scanner_get_next_token (scanner);

  /*
   * Are there any options to limit the scope of the definition?
   */

  if (token == G_TOKEN_LEFT_BRACE)
    {
      g_scanner_set_scope (scanner, 1);
      scanner->config->int_2_float = 0;
      modifier = buttons = 0;
      while ((token = g_scanner_get_next_token (scanner))
             != G_TOKEN_RIGHT_BRACE)
        {
          if (token == G_TOKEN_SYMBOL)
            {
              if ((intptr_t) scanner->value.v_symbol < 11)
                 buttons |= 1 << ((intptr_t) scanner->value.v_symbol - 1);
              else
                 modifier |= 1 << ((intptr_t) scanner->value.v_symbol - 11);
            }
          else if (token == G_TOKEN_INT)
            {
              if (scanner->value.v_int <= 5 && scanner->value.v_int > 0)
                buttons |= 1 << (scanner->value.v_int - 1);
              else
                g_printerr ("Only Buttons 1-5 are supported!\n");
            }
          else
            {
              g_printerr ("skipped token\n");
            }
        }
      g_scanner_set_scope (scanner, 0);
      scanner->config->int_2_float = 1;
      token = g_scanner_get_next_token (scanner);
    }

  name [len] = 124;
  name [len+1] = buttons + 64;
  name [len+2] = modifier + 48;
  name [len+3] = 0;

  return name;
}

void parse_config (GromitData *data)
{
  GromitPaintContext *context=NULL;
  GromitPaintContext *context_template=NULL;
  GScanner *scanner;
  GTokenType token;
  gchar *filename;
  int file;

  gchar *name, *copy;

  GromitPaintType type;
  GdkRGBA *fg_color=NULL;
  guint width, arrowsize, minwidth;

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
      g_printerr("No usable config found, leaving tools unconfigured!\n");
      g_free(filename);
      return;
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
  g_scanner_scope_add_symbol (scanner, 0, "ERASER", (gpointer) GROMIT_ERASER);
  g_scanner_scope_add_symbol (scanner, 0, "RECOLOR",(gpointer) GROMIT_RECOLOR);
  g_scanner_scope_add_symbol (scanner, 0, "HOTKEY",            HOTKEY_SYMBOL_VALUE);
  g_scanner_scope_add_symbol (scanner, 0, "UNDOKEY",           UNDOKEY_SYMBOL_VALUE);

  g_scanner_scope_add_symbol (scanner, 1, "BUTTON1", (gpointer) 1);
  g_scanner_scope_add_symbol (scanner, 1, "BUTTON2", (gpointer) 2);
  g_scanner_scope_add_symbol (scanner, 1, "BUTTON3", (gpointer) 3);
  g_scanner_scope_add_symbol (scanner, 1, "BUTTON4", (gpointer) 4);
  g_scanner_scope_add_symbol (scanner, 1, "BUTTON5", (gpointer) 5);
  g_scanner_scope_add_symbol (scanner, 1, "SHIFT",   (gpointer) 11);
  g_scanner_scope_add_symbol (scanner, 1, "CONTROL", (gpointer) 12);
  g_scanner_scope_add_symbol (scanner, 1, "META",    (gpointer) 13);
  g_scanner_scope_add_symbol (scanner, 1, "ALT",     (gpointer) 13);

  g_scanner_scope_add_symbol (scanner, 2, "size",      (gpointer) 1);
  g_scanner_scope_add_symbol (scanner, 2, "color",     (gpointer) 2);
  g_scanner_scope_add_symbol (scanner, 2, "arrowsize", (gpointer) 3);
  g_scanner_scope_add_symbol (scanner, 2, "minsize",   (gpointer) 4);

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

          name = parse_name (scanner);
          token = g_scanner_cur_token(scanner);

          if (token != G_TOKEN_EQUAL_SIGN)
            {
              g_scanner_unexp_token (scanner, G_TOKEN_EQUAL_SIGN, NULL,
                                     NULL, NULL, "aborting", TRUE);
              exit (1);
            }

          token = g_scanner_get_next_token (scanner);

          /* defaults */

          type = GROMIT_PEN;
          width = 7;
          arrowsize = 0;
          minwidth = 1;
          fg_color = data->red;

          if (token == G_TOKEN_SYMBOL)
            {
              type = (GromitPaintType) scanner->value.v_symbol;
              token = g_scanner_get_next_token (scanner);
            }
          else if (token == G_TOKEN_STRING)
            {
              copy = parse_name (scanner);
              token = g_scanner_cur_token(scanner);
              context_template = g_hash_table_lookup (data->tool_config, copy);
              if (context_template)
                {
                  type = context_template->type;
                  width = context_template->width;
                  arrowsize = context_template->arrowsize;
                  minwidth = context_template->minwidth;
                  fg_color = context_template->paint_color;
                }
              else
                {
                  g_printerr ("WARNING: Unable to copy \"%s\": "
                              "not yet defined!\n", copy);
                }
            }
          else
            {
              g_printerr ("Expected Tool-definition "
                          "or name of template tool\n");
              exit (1);
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
                              exit (1);
                            }
                          token = g_scanner_get_next_token (scanner);
                          if (token != G_TOKEN_FLOAT)
                            {
                              g_printerr ("Missing Size (float)... aborting\n");
                              exit (1);
                            }
                          width = (guint) (scanner->value.v_float + 0.5);
                        }
                      else if ((intptr_t) scanner->value.v_symbol == 2)
                        {
                          token = g_scanner_get_next_token (scanner);
                          if (token != G_TOKEN_EQUAL_SIGN)
                            {
                              g_printerr ("Missing \"=\"... aborting\n");
                              exit (1);
                            }
                          token = g_scanner_get_next_token (scanner);
                          if (token != G_TOKEN_STRING)
                            {
                              g_printerr ("Missing Color (string)... "
                                          "aborting\n");
                              exit (1);
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
                              exit (1);
                            }
                          token = g_scanner_get_next_token (scanner);
                          if (token != G_TOKEN_FLOAT)
                            {
                              g_printerr ("Missing Arrowsize (float)... "
                                          "aborting\n");
                              exit (1);
                            }
                          arrowsize = scanner->value.v_float;
                        }
                      else if ((intptr_t) scanner->value.v_symbol == 4)
                        {
                          token = g_scanner_get_next_token (scanner);
                          if (token != G_TOKEN_EQUAL_SIGN)
                            {
                              g_printerr ("Missing \"=\"... aborting\n");
                              exit (1);
                            }
                          token = g_scanner_get_next_token (scanner);
                          if (token != G_TOKEN_FLOAT)
                            {
                              g_printerr ("Missing Minsize (float)... "
                                          "aborting\n");
                              exit (1);
                            }
                          minwidth = scanner->value.v_float;
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
              exit (1);
            }

          context = paint_context_new (data, type, fg_color, width, arrowsize, minwidth);
          g_hash_table_insert (data->tool_config, name, context);
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
              exit (1);
            }

          token = g_scanner_get_next_token (scanner);

          if (token != G_TOKEN_STRING)
            {
              g_scanner_unexp_token (scanner, G_TOKEN_STRING, NULL,
                                     NULL, NULL, "aborting", TRUE);
              exit (1);
            }

          if (key_type == HOTKEY_SYMBOL_VALUE && g_strcmp0(data->hot_keyval, DEFAULT_HOTKEY) == 0)
            {
              data->hot_keyval = g_strdup(scanner->value.v_string);
            }
          else if (key_type == UNDOKEY_SYMBOL_VALUE && g_strcmp0(data->undo_keyval, DEFAULT_UNDOKEY) == 0)
            {
              data->undo_keyval = g_strdup(scanner->value.v_string);
            }

          token = g_scanner_get_next_token(scanner);

          if (token != ';')
            {
              g_printerr ("Expected \";\"\n");
              exit (1);
            }
        }
      else
        {
          g_printerr ("Expected name of Tool to define or Hot key definition\n");
          exit(1);
        }

      token = g_scanner_get_next_token (scanner);
    }
  g_scanner_destroy (scanner);
  close (file);
  g_free (filename);
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
