/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */
#include "coil.h"

int main(int argc, char **argv)
{
  CoilStruct *root = NULL;
  GString    *buffer = g_string_sized_new(8192);
  GError     *internal_error = NULL;

  coil_init();

  if (argc > 1)
  {
    int i;
    for (i = 1; i < argc; i++)
    {
      if (!g_file_test(argv[i], G_FILE_TEST_EXISTS |
                                G_FILE_TEST_IS_REGULAR))
          g_error("Error: file '%s' does not exist.", argv[i]);

      root = coil_parse_file(argv[i], &internal_error);

      if (G_UNLIKELY(internal_error))
      {
        g_printerr("Continuing despite errors: %s",
                   internal_error->message);

        g_error_free(internal_error);
        internal_error = NULL;
      }

      if (root)
      {
        coil_struct_build_string(root, buffer,
                                 &default_string_format,
                                 &internal_error);

        if (G_UNLIKELY(internal_error))
          goto error;

        g_print("%s\n", buffer->str);
      }

      g_string_truncate(buffer, 0);
      g_object_unref(root);
      root = NULL;
    }
  }
  else
  {
    root = coil_parse_stream(stdin, NULL, &internal_error);

    if (root)
    {
      coil_struct_build_string(root, buffer,
                               &default_string_format,
                               &internal_error);

      if (G_UNLIKELY(internal_error))
        goto error;

      g_print("%s", buffer->str);
    }

    if (G_UNLIKELY(internal_error))
      goto error;

    g_object_unref(root);
    root = NULL;
  }

  g_string_free(buffer, TRUE);

  return 0;

error:
  if (root)
    g_object_unref(root);

  g_string_free(buffer, TRUE);

  if (internal_error)
  {
    g_printerr("Error: %s\n", internal_error->message);
    g_error_free(internal_error);
  }

  return 0;
}

