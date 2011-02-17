/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */
#ifndef _COIL_COMMON_H
#define _COIL_COMMON_H

#define _GNU_SOURCE

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifndef COIL_DEBUG
#  define G_DISABLE_CHECKS 1
#endif

#include "glib.h"
#include "glib-object.h"
#include "error.h"

#define COIL_API(rtype) rtype
#define COIL_STATIC_STRLEN(str) str,(sizeof(str)-1)

G_BEGIN_DECLS

void
coil_init(void);

G_END_DECLS

#endif
