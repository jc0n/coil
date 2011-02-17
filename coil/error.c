/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#include "common.h"
#include "error.h"

/*
 * coil_error_quark:
 *
 * Return error identifier for Glib Error Handling
 */
GQuark coil_error_quark(void)
{
  static GQuark result = 0;

  if (!result)
    result = g_quark_from_static_string("coil-error-quark");

  return result;
}

