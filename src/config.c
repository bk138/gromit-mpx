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
#include "math.h"
#include "build-config.h"

#define KEY_DFLT_SHOW_INTRO_ON_STARTUP TRUE

#define KEYFILE_FLAGS G_KEY_FILE_KEEP_COMMENTS|G_KEY_FILE_KEEP_TRANSLATIONS

static gpointer HOTKEY_SYMBOL_VALUE  = (gpointer) 3;
static gpointer UNDOKEY_SYMBOL_VALUE = (gpointer) 4;

/*
 * IMPORTANT RULE: the color field in GromitPaintContext is
 * _always_ individually allocated, i.e. GdkRGBA pointers all point to
 * separate copies.
 */

/*
 * initialize GScanner for the parsing of tool definitions
 */
void scanner_init(GScanner *scanner)
{
  scanner->config->case_sensitive = 0;
  scanner->config->scan_octal = 0;
  scanner->config->identifier_2_string = 0;
  scanner->config->char_2_token = 1;
  scanner->config->numbers_2_int = 1;
  scanner->config->int_2_float = 1;

  g_scanner_scope_add_symbol (scanner, 0, "PEN",       (gpointer) GROMIT_PEN);
  g_scanner_scope_add_symbol (scanner, 0, "LINE",      (gpointer) GROMIT_LINE);
  g_scanner_scope_add_symbol (scanner, 0, "RECT",      (gpointer) GROMIT_RECT);
  g_scanner_scope_add_symbol (scanner, 0, "SMOOTH",    (gpointer) GROMIT_SMOOTH);
  g_scanner_scope_add_symbol (scanner, 0, "ORTHOGONAL",(gpointer) GROMIT_ORTHOGONAL);
  g_scanner_scope_add_symbol (scanner, 0, "ERASER",    (gpointer) GROMIT_ERASER);
  g_scanner_scope_add_symbol (scanner, 0, "RECOLOR",   (gpointer) GROMIT_RECOLOR);
  g_scanner_scope_add_symbol (scanner, 0, "HOTKEY",               HOTKEY_SYMBOL_VALUE);
  g_scanner_scope_add_symbol (scanner, 0, "UNDOKEY",              UNDOKEY_SYMBOL_VALUE);

  g_scanner_scope_add_symbol (scanner, 1, "BUTTON1", (gpointer) 1);
  g_scanner_scope_add_symbol (scanner, 1, "BUTTON2", (gpointer) 2);
  g_scanner_scope_add_symbol (scanner, 1, "BUTTON3", (gpointer) 3);
  g_scanner_scope_add_symbol (scanner, 1, "BUTTON4", (gpointer) 4);
  g_scanner_scope_add_symbol (scanner, 1, "BUTTON5", (gpointer) 5);
  g_scanner_scope_add_symbol (scanner, 1, "SHIFT",   (gpointer) 11);
  g_scanner_scope_add_symbol (scanner, 1, "CONTROL", (gpointer) 12);
  g_scanner_scope_add_symbol (scanner, 1, "META",    (gpointer) 13);
  g_scanner_scope_add_symbol (scanner, 1, "ALT",     (gpointer) 13);

  g_scanner_scope_add_symbol (scanner, 2, "size",      (gpointer) SYM_SIZE);
  g_scanner_scope_add_symbol (scanner, 2, "color",     (gpointer) SYM_COLOR);
  g_scanner_scope_add_symbol (scanner, 2, "arrowsize", (gpointer) SYM_ARROWSIZE);
  g_scanner_scope_add_symbol (scanner, 2, "arrowtype", (gpointer) SYM_ARROWTYPE);
  g_scanner_scope_add_symbol (scanner, 2, "minsize",   (gpointer) SYM_MINSIZE);
  g_scanner_scope_add_symbol (scanner, 2, "maxsize",   (gpointer) SYM_MAXSIZE);
  g_scanner_scope_add_symbol (scanner, 2, "radius",    (gpointer) SYM_RADIUS);
  g_scanner_scope_add_symbol (scanner, 2, "maxangle",  (gpointer) SYM_MAXANGLE);
  g_scanner_scope_add_symbol (scanner, 2, "minlen",    (gpointer) SYM_MINLEN);
  g_scanner_scope_add_symbol (scanner, 2, "simplify",  (gpointer) SYM_SIMPLIFY);
  g_scanner_scope_add_symbol (scanner, 2, "snap",      (gpointer) SYM_SNAP);

  g_scanner_set_scope (scanner, 0);
  scanner->config->scope_0_fallback = 0;
}

/*
 * returns the name of the tool, or NULL
 * the string returned is allocated and then owned by the caller
 */
gchar* parse_name (GScanner *scanner)
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
      return NULL;
    }

  len = strlen (scanner->value.v_string);
  name = g_strndup (scanner->value.v_string, len + 3);
  token = g_scanner_get_next_token (scanner);

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

/*
 * get the "type" of the tool, e.g. PEN (e.g. =PEN), or a base tool
 * style that is inherited (e.g. ="red pen") and store characteristics
 * in style.
 *
 * allocates a new GdkRGBA color
 *
 * returns FALSE upon error
 */
gboolean parse_tool(GromitData *data, GScanner *scanner, GromitPaintContext *style)
{
  GTokenType token = g_scanner_cur_token(scanner);
  gboolean color_allocated = FALSE;

  if (token != G_TOKEN_EQUAL_SIGN)
    {
      g_scanner_unexp_token(
          scanner, G_TOKEN_EQUAL_SIGN, NULL, NULL, NULL, "aborting", TRUE);
      goto cleanup;
    }

  token = g_scanner_get_next_token (scanner);

  // style defaults
  style->type = GROMIT_PEN;
  style->width = 7;
  style->arrowsize = 0;
  style->arrow_type = GROMIT_ARROW_END;
  style->radius = 10;
  style->minlen = style->radius * 5 / 2;
  style->maxangle = 15;
  style->simplify = 10;
  style->snapdist = 0;
  style->minwidth = 1;
  style->maxwidth = G_MAXUINT;

  // allocate new color and copy default red fields
  style->paint_color = g_malloc(sizeof (GdkRGBA));
  color_allocated = TRUE;
  *style->paint_color = *data->red;

  if (token == G_TOKEN_SYMBOL)
    {
      style->type = (GromitPaintType) scanner->value.v_symbol;
      token = g_scanner_get_next_token (scanner);
    }
  else if (token == G_TOKEN_STRING)
    {
      gchar *copy = parse_name (scanner);
      g_printerr("parse_tool: string: %s\n", copy);
      if(!copy)
          goto cleanup;
      token = g_scanner_cur_token(scanner);
      GromitPaintContext *context = g_hash_table_lookup (data->tool_config, copy);
      if (context)
        {
          // copy fields of context that is inherited from
          style->type = context->type;
          style->width = context->width;
          style->arrowsize = context->arrowsize;
          style->arrow_type = context->arrow_type;
          style->radius = context->radius;
          style->minlen = context->minlen;
          style->maxangle = context->maxangle;
          style->simplify = context->simplify;
          style->snapdist = context->snapdist;
          style->minwidth = context->minwidth;
          style->maxwidth = context->maxwidth;
          *style->paint_color = *context->paint_color;

          // nullify superfluous fields, although not really necessary
          style->paint_ctx  = NULL;
          style->pressure = 0;
        }
      else
        {
          g_printerr ("WARNING: Unable to copy \"%s\": "
                      "not yet defined!\n", copy);
        }
    }
  else
    {
      g_printerr ("Expected tool definition or name of template tool\n");
      goto cleanup;
    }
  return TRUE;

 cleanup:
  if (color_allocated)
    {
      g_free(style->paint_color);
      style->paint_color = NULL;
    }
  return FALSE;
}

/*
 * parse a single attribute (e.g. size=6) and store it in style,
 * returning the attribute ID (here SYM_SIZE), or SYM_ERR if something
 * went wrong.
 */
ToolAttribute parse_attribute(GScanner *scanner, GromitPaintContext *style)
{
  const gint id = (intptr_t) scanner->value.v_symbol;
  GTokenType token;
  if (id == SYM_SIZE)
    {
      gfloat v = parse_get_float(scanner, "Missing size");
      if (isnan(v)) return SYM_ERROR;
      style->width = v + 0.5;
    }
  else if (id == SYM_COLOR)
    {
      token = g_scanner_get_next_token (scanner);
      if (token != G_TOKEN_EQUAL_SIGN)
        {
          g_printerr ("Missing \"=\"... aborting\n");
          return SYM_ERROR;
        }
      token = g_scanner_get_next_token (scanner);
      if (token != G_TOKEN_STRING)
        {
          g_printerr ("Missing Color (string)... aborting\n");
          return SYM_ERROR;
        }
      if (! gdk_rgba_parse (style->paint_color, scanner->value.v_string))
        {
          g_printerr ("Unable to parse color. Keeping default.\n");
        }
    }
  else if (id == SYM_ARROWSIZE)
    {
        gfloat v = parse_get_float(scanner, "Missing arrowsize");
        if (isnan(v)) return SYM_ERROR;
        style->arrowsize = v;
    }
  else if (id == SYM_ARROWTYPE)
    {
      token = g_scanner_get_next_token (scanner);
      if (token != G_TOKEN_EQUAL_SIGN)
        {
          g_printerr ("Missing \"=\"... aborting\n");
          return SYM_ERROR;
        }
      token = g_scanner_get_next_token (scanner);
      if (token != G_TOKEN_STRING)
        {
          g_printerr ("Missing arrowtype (string)... aborting\n");
          return SYM_ERROR;
        }
      if (! strcasecmp(scanner->value.v_string, "end"))
        {
          style->arrow_type = GROMIT_ARROW_END;
        }
      else if (! strcasecmp(scanner->value.v_string, "start"))
        {
          style->arrow_type = GROMIT_ARROW_START;
        }
      else if (! strcasecmp(scanner->value.v_string, "double"))
        {
          style->arrow_type = GROMIT_ARROW_DOUBLE;
        }
      else
        {
          g_printerr ("Arrow type must be \"start\", \"end\", or \"double\"... "
                      "aborting\n");
          return SYM_ERROR;
        }
    }
  else if ((intptr_t) scanner->value.v_symbol == SYM_RADIUS)
    {
      gfloat v = parse_get_float(scanner, "Missing radius (float)");
      if (isnan(v)) return SYM_ERROR;
      style->radius = v;
    }
  else if ((intptr_t) scanner->value.v_symbol == SYM_MAXANGLE)
    {
      gfloat v = parse_get_float(scanner, "Missing angle (float)");
      if (isnan(v)) return SYM_ERROR;
      style->maxangle = v;
    }
  else if ((intptr_t) scanner->value.v_symbol == SYM_SIMPLIFY)
    {
      gfloat v = parse_get_float(scanner, "Missing simplify value (float)");
      if (isnan(v)) return SYM_ERROR;
      style->simplify = v;
    }
  else if ((intptr_t) scanner->value.v_symbol == SYM_MINLEN)
    {
      gfloat v = parse_get_float(scanner, "Missing minlen value (float)");
      if (isnan(v)) return SYM_ERROR;
      style->minlen = v;
    }
  else if ((intptr_t) scanner->value.v_symbol == SYM_SNAP)
    {
      gfloat v = parse_get_float(scanner, "Missing snap distance (float)");
      if (isnan(v)) return SYM_ERROR;
      style->snapdist = v;
    }
  else if (id == SYM_MINSIZE)
    {
      gfloat v = parse_get_float(scanner, "Missing minsize");
      if (isnan(v)) return SYM_ERROR;
      style->minwidth = v + 0.5;
    }
  else if (id == SYM_MAXSIZE)
    {
      gfloat v = parse_get_float(scanner, "Missing maxsize");
      if (isnan(v)) return SYM_ERROR;
      style->maxwidth = v + 0.5;
    }
  else
    {
      g_printerr ("Unknown tool type?????\n");
      return SYM_ERROR;
    }
  return id;
}

/*
 * get "=VALUE", where VALUE is a float
 * returns NAN is an error occurs
 */
gfloat parse_get_float(GScanner *scanner, const gchar *msg)
{
  GTokenType token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    {
      g_printerr ("Missing \"=\"... aborting\n");
      return NAN;
    }
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_FLOAT)
    {
      g_printerr ("%s", msg);
      g_printerr ("... aborting\n");
      return NAN;
    }
  return scanner->value.v_float;
}
/*
 * parses a pen style definition (e.g. (color="red" size=3) )
 * and stores fields found in GromitStyleDef.
 *
 * returns FALSE upon any error
 */
gboolean parse_style(GScanner *scanner, GromitPaintContext *style)
{
  g_scanner_set_scope (scanner, 2);
  scanner->config->int_2_float = 1;
  GTokenType token = g_scanner_get_next_token (scanner);

  while (token != G_TOKEN_RIGHT_PAREN)
    {
      if (token == G_TOKEN_SYMBOL)
        {
          if (parse_attribute(scanner, style) == SYM_ERROR)
            return FALSE;
        }
      token = g_scanner_get_next_token (scanner);
    }
  g_scanner_set_scope (scanner, 0);
  token = g_scanner_get_next_token (scanner);
  return TRUE;
}

/*
 * parse config file
 */
gboolean parse_config (GromitData *data)
{
  gboolean status = FALSE;
  GromitPaintContext *context=NULL;
  GScanner *scanner;
  GTokenType token;
  gchar *filename;
  int file;
  gchar *name;

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
  scanner_init(scanner);
  scanner->input_name = filename;
  g_scanner_input_file (scanner, file);
  token = g_scanner_get_next_token (scanner);

  GromitPaintContext style;

  while (token != G_TOKEN_EOF)
    {
      if (token == G_TOKEN_STRING)
        {
          // New tool definition
          name = parse_name (scanner);
	  if(!name)
	      goto cleanup;

          if (!parse_tool(data, scanner, &style))
            {
              g_printerr("parse tool failed\n");
              goto cleanup;
            }

          //  are there any tool-options?
          token = g_scanner_cur_token(scanner);
          if (token == G_TOKEN_LEFT_PAREN)
            {
              if (! parse_style(scanner, &style))
                goto cleanup;
            }

          // finally we expect a semicolon
          token = g_scanner_cur_token (scanner);
          if (token != ';')
            {
              g_printerr ("Expected \";\"\n");
              goto cleanup;
            }

          context = paint_context_new(data, style.type, style.paint_color, style.width,
                                      style.arrowsize, style.arrow_type,
                                      style.simplify, style.radius, style.maxangle, style.minlen, style.snapdist,
                                      style.minwidth, style.maxwidth);
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
