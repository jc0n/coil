/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#ifndef COIL_LIST_H
#define COIL_LIST_H

#include "value.h"

#define COIL_TYPE_LIST G_TYPE_VALUE_ARRAY
#define CoilList GValueArray

G_BEGIN_DECLS

void
coil_list_build_string(CoilList *list, GString *buffer, CoilStringFormat *format);

gchar *
coil_list_to_string(CoilList *list, CoilStringFormat *format);

G_END_DECLS

#endif
