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
#include "parser.h"
#include "main.h"
#include "build-config.h"

gpointer HOTKEY_SYMBOL_VALUE  = (gpointer) 3;
gpointer UNDOKEY_SYMBOL_VALUE = (gpointer) 4;

/*
 * set scanner to default initial values
 */

void scanner_init(GScanner* scanner)
{
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
  g_scanner_scope_add_symbol (scanner, 2, "maxsize",   (gpointer) 5);

  g_scanner_set_scope (scanner, 0);
  scanner->config->scope_0_fallback = 0;
}

/*
 * get name of tool, e.g. "default"[SHIFT], which is returned as "defaukt|@1"
 * returns NULL upon error
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
 * returns FALSE upon error
 */

gboolean parse_tool(GromitData *data, GScanner *scanner, GromitStyleDef *style)
{
  GTokenType token = g_scanner_cur_token(scanner);

  if (token != G_TOKEN_EQUAL_SIGN)
    {
      g_scanner_unexp_token(
          scanner, G_TOKEN_EQUAL_SIGN, NULL, NULL, NULL, "aborting", TRUE);
      return FALSE;
    }

  token = g_scanner_get_next_token(scanner);

  /* defaults */
  style->type = GROMIT_PEN;
  style->width = 7;
  style->arrowsize = 0;
  style->minwidth = 1;
  style->maxwidth = G_MAXUINT;

  GdkRGBA *red_color = g_malloc(sizeof (GdkRGBA));
  *red_color = *data->red;
  style->paint_color = red_color;

  if (token == G_TOKEN_SYMBOL)
    {
      style->type = (GromitPaintType)scanner->value.v_symbol;
      token = g_scanner_get_next_token(scanner);
    }
  else if (token == G_TOKEN_STRING)
    {
      gchar *copy = parse_name(scanner);
      if (!copy)
        return FALSE;
      token = g_scanner_cur_token(scanner);
      GromitPaintContext *context = g_hash_table_lookup(data->tool_config, copy);
      if (context)
        {
          style->type = context->type;
          style->width = context->width;
          style->arrowsize = context->arrowsize;
          style->minwidth = context->minwidth;
          style->maxwidth = context->maxwidth;
          style->paint_color = context->paint_color;
        }
      else
        {
          g_printerr(
              "WARNING: Unable to copy \"%s\": "
              "not yet defined!\n",
              copy);
        }
    }
  else
    {
      g_printerr(
          "Expected Tool-definition "
          "or name of template tool\n");
      return FALSE;
    }
  return TRUE;
}

/*
 * parses a pen style definition (e.g. (color="red" size=3) )
 * and stores fields found in GromitStyleDef.
 *
 * returns FALSE upon any error
 */

gboolean parse_style(GScanner *scanner, GromitStyleDef *style)
{
  GdkRGBA *color = NULL;
  g_scanner_set_scope(scanner, 2);
  scanner->config->int_2_float = 1;
  GTokenType token = g_scanner_get_next_token(scanner);
  while (token != G_TOKEN_RIGHT_PAREN)
    {
      if (token == G_TOKEN_SYMBOL)
        {
          if ((intptr_t)scanner->value.v_symbol == 1)
            {
              token = g_scanner_get_next_token(scanner);
              if (token != G_TOKEN_EQUAL_SIGN)
                {
                  g_printerr("Missing \"=\"... aborting\n");
                  return FALSE;
                }
              token = g_scanner_get_next_token(scanner);
              if (token != G_TOKEN_FLOAT)
                {
                  g_printerr("Missing Size (float)... aborting\n");
                  return FALSE;
                }
              style->width = (guint)(scanner->value.v_float + 0.5);
            }
          else if ((intptr_t)scanner->value.v_symbol == 2)
            {
              token = g_scanner_get_next_token(scanner);
              if (token != G_TOKEN_EQUAL_SIGN)
                {
                  g_printerr("Missing \"=\"... aborting\n");
                  return FALSE;
                }
              token = g_scanner_get_next_token(scanner);
              if (token != G_TOKEN_STRING)
                {
                  g_printerr(
                      "Missing Color (string)... "
                      "aborting\n");
                  return FALSE;
                }
              color = g_malloc(sizeof(GdkRGBA));
              if (gdk_rgba_parse(color, scanner->value.v_string))
                {
                  style->paint_color = color;
                }
              else
                {
                  g_printerr(
                      "Unable to parse color. "
                      "Keeping default.\n");
                  g_free(color);
                }
              color = NULL;
            }
          else if ((intptr_t)scanner->value.v_symbol == 3)
            {
              token = g_scanner_get_next_token(scanner);
              if (token != G_TOKEN_EQUAL_SIGN)
                {
                  g_printerr("Missing \"=\"... aborting\n");
                  return FALSE;
                }
              token = g_scanner_get_next_token(scanner);
              if (token != G_TOKEN_FLOAT)
                {
                  g_printerr(
                      "Missing Arrowsize (float)... "
                      "aborting\n");
                  return FALSE;
                }
              style->arrowsize = scanner->value.v_float;
            }
          else if ((intptr_t)scanner->value.v_symbol == 4)
            {
              token = g_scanner_get_next_token(scanner);
              if (token != G_TOKEN_EQUAL_SIGN)
                {
                  g_printerr("Missing \"=\"... aborting\n");
                  return FALSE;
                }
              token = g_scanner_get_next_token(scanner);
              if (token != G_TOKEN_FLOAT)
                {
                  g_printerr(
                      "Missing Minsize (float)... "
                      "aborting\n");
                  return FALSE;
                }
              style->minwidth = scanner->value.v_float;
            }
          else if ((intptr_t)scanner->value.v_symbol == 5)
            {
              token = g_scanner_get_next_token(scanner);
              if (token != G_TOKEN_EQUAL_SIGN)
                {
                  g_printerr("Missing \"=\"... aborting\n");
                  return FALSE;
                }
              token = g_scanner_get_next_token(scanner);
              if (token != G_TOKEN_FLOAT)
                {
                  g_printerr(
                      "Missing Maxsize (float)... "
                      "aborting\n");
                  return FALSE;
                }
              style->maxwidth = scanner->value.v_float;
            }
          else
            {
              g_printerr("Unknown tool type ! \n");
              return FALSE;
            }
        }
      else
        {
          g_printerr("Unknown token: %d !\n", token);
          return FALSE;
        }
      token = g_scanner_get_next_token(scanner);
    }  // while (token != G_TOKEN_RIGHT_PAREN)
  g_scanner_set_scope(scanner, 0);
  g_scanner_get_next_token(scanner);
  return TRUE;
}
