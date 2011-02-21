/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#ifndef _COIL_LIST_H
#define COIL_LIST_H

#include "value.h"

#define COIL_TYPE_LIST (coil_list_get_type())

G_BEGIN_DECLS

GType
coil_list_get_type(void) G_GNUC_CONST;

/*void
coil_list_build_string_buffer(GString *const buffer,
                              const GList *list);
*/


void
coil_list_build_string(const GList      *list,
                       GString          *const buffer,
                       CoilStringFormat *format,
                       GError          **error);

gchar *
coil_list_to_string(const GList      *list,
                    CoilStringFormat *format,
                    GError          **error);

G_END_DECLS

#endif
