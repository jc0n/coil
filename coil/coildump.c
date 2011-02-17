/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#include "coil.h"

int
main (int argc, char **argv)
{
  GError *error = NULL;

  coil_parse_stream (stdin, &error);

  if (error)
    g_error("Error: %s\n", error->message);

  exit (EXIT_SUCCESS);
}
