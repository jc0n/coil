/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#include "coil.h"

#include <stdlib.h>
#include <string.h>

static gchar **files = NULL;
static gchar **attributes = NULL;
static gchar **blocks = NULL;
static gchar *expand = NULL;
static gboolean flatten = FALSE;
static gboolean permissive = FALSE;
static gboolean compat = FALSE;
static gboolean merge_files = FALSE;
static gboolean noclobber_attributes = FALSE;

static GOptionEntry entries[] =
{
  {"attribute", 'a', 0, G_OPTION_ARG_STRING_ARRAY, &attributes,
      "A path.key:value pair to add to the coil output", NULL},

  {"block", 'b', 0, G_OPTION_ARG_STRING_ARRAY, &blocks,
      "Show coil contained within a struct at path P", "P"},

  {"flatten", 'f', 0, G_OPTION_ARG_NONE, &flatten,
      "Show key-values on separate, fully-qualified lines", NULL},

  {"expand", 0, 0, G_OPTION_ARG_STRING, &expand,
      "Control expanded types. Prefix types with - to remove. Default is all." \
      " Valid types are all, strings", NULL},

  {"permissive", 0, 0, G_OPTION_ARG_NONE, &permissive,
      "Ignore minor errors during parsing.", NULL},

  {"compat", 0, 0, G_OPTION_ARG_NONE, &compat,
      "Maintain compatability with previous coil versions.", NULL},

  {"merge-files", 'm', 0, G_OPTION_ARG_NONE, &merge_files,
      "Merge all files specified into one struct.", NULL},

  {"no-clobber", 'n', 0, G_OPTION_ARG_NONE, &noclobber_attributes,
      "Don't overwrite existing attributes with those specified on the " \
      "command line.", NULL},

  {G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &files, NULL, NULL},

  {NULL}
};

static void
parse_options(int *argc,
              char ***argv)
{
  GError         *error = NULL;
  GOptionContext *option_context;

  option_context = g_option_context_new("file1.coil [file2.coil] ...");
  g_option_context_add_main_entries(option_context, entries, NULL);

  if (!g_option_context_parse(option_context, argc, argv, &error))
  {
    g_printerr("Failed parsing options: %s\n", error->message);
    g_error_free(error);
    exit(EXIT_FAILURE);
  }

  if (files == NULL)
  {
    g_printerr("No files specified.\n");
    exit(EXIT_FAILURE);
  }

  g_option_context_free(option_context);
}

static CoilStruct *
parse_attributes(GError **error)
{
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  gchar      *buf;
  CoilStruct *result = NULL;

  if (attributes)
  {
    buf = g_strjoinv(" ", attributes);
    result = coil_parse_string(buf, error);
    g_free(buf);
  }

  return result;
}

static void
print_blocks(CoilStruct       *root,
             GString          *buffer,
             CoilStringFormat *format,
             GError          **error)
{
  GError *internal_error = NULL;
  gint    i = 0;

  for (i = 0; blocks[i]; i++)
  {
    const GValue *value;
    CoilStruct   *block;
    const gchar  *path;
    guint         len;

    g_strstrip(blocks[i]);

    path = blocks[i];
    len = strlen(path);

    value = coil_struct_lookup(root, path, len, TRUE, &internal_error);

    if (G_UNLIKELY(internal_error))
    {
      g_propagate_error(error, internal_error);
      return;
    }

    if (value)
    {
      if (!G_VALUE_HOLDS(value, COIL_TYPE_STRUCT))
      {
        g_set_error(error,
                    COIL_ERROR,
                    COIL_ERROR_VALUE,
                    "Values for '--block' must be type struct. Path '%s' is type '%s'",
                    path, G_VALUE_TYPE_NAME(value));

        return;
      }

      block = COIL_STRUCT(g_value_dup_object(value));
      coil_struct_build_string(block, buffer, format, &internal_error);

      if (G_UNLIKELY(internal_error))
      {
        g_object_unref(block);
        g_propagate_error(error, internal_error);
        return;
      }

      g_object_unref(block);
    }
  }
}

static void
print_struct(CoilStruct       *node,
             GString          *buffer,
             CoilStringFormat *format,
             GError          **error)
{
  GError *internal_error = NULL;

  if (blocks)
    print_blocks(node, buffer, format, &internal_error);
  else
    coil_struct_build_string(node, buffer, format, &internal_error);

  if (G_UNLIKELY(internal_error))
  {
    g_propagate_error(error, internal_error);
    return;
  }

  g_print("%s\n", buffer->str);
}

static void
print_files(void)
{
  CoilStruct      *root = NULL, *attrs = NULL, *parsed = NULL;
  GString         *buffer = g_string_sized_new(8192);
  GError          *error = NULL;
  CoilStringFormat format = default_string_format;
  gboolean         overwrite = !noclobber_attributes;
  gint             i;

  root = coil_struct_new(NULL, NULL);

  if (flatten)
    format.options |= FLATTEN_PATHS;

  if (compat)
    format.options |= LEGACY;

  attrs = parse_attributes(&error);

  if (G_UNLIKELY(error))
    goto error;

  for (i = 0; files[i]; i++)
  {
    if (strcmp(files[i], "-") == 0)
      parsed = coil_parse_stream(stdin, NULL, &error);
    else
      parsed = coil_parse_file(files[i], &error);

    if (G_UNLIKELY(error))
    {
      if (permissive)
      {
        g_printerr("Continuing despite errors: %s", error->message);
        g_error_free(error);
        error = NULL;

        if (parsed)
        {
          g_object_unref(parsed);
          parsed = NULL;
        }

        continue;
      }

      goto error;
    }

    /* TODO(jcon): Modify parser to accept pre-existing root */
    if (merge_files)
    {
      if (!coil_struct_merge(parsed, root, FALSE, &error))
        goto error;
    }
    else
    {
      if (attrs && !coil_struct_merge(attrs, parsed, overwrite, &error))
        goto error;

      print_struct(parsed, buffer, &format, &error);
      g_string_truncate(buffer, 0);
    }

    g_object_unref(parsed);
    parsed = NULL;
  }

  if (merge_files)
  {
    if (attrs
      && !coil_struct_merge(attrs, root, overwrite, &error))
      goto error;

    print_struct(root, buffer, &format, &error);
  }

  g_object_unref(root);

  if (attrs)
    g_object_unref(attrs);

  g_string_free(buffer, TRUE);

  return;

error:
  if (root)
    g_object_unref(root);

  if (parsed)
    g_object_unref(parsed);

  if (attrs)
    g_object_unref(attrs);

  if (error)
  {
    g_printerr("%s\n", error->message);
    g_error_free(error);
  }

  g_string_free(buffer, TRUE);
  exit(EXIT_FAILURE);
}

int
main (int argc,
      char **argv)
{
  parse_options(&argc, &argv);

  coil_init();

  print_files();

  exit (EXIT_SUCCESS);
}

