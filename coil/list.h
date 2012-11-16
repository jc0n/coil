/*
 * Copyright (C) 2012 John O'Connor
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef COIL_LIST_H
#define COIL_LIST_H

#include "value.h"

#define COIL_TYPE_LIST          \
    (coil_list_get_type())

#define COIL_LIST(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), COIL_TYPE_LIST, CoilList))

#define COIL_IS_LIST(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), COIL_TYPE_LIST))

#define COIL_LIST_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), COIL_TYPE_LIST, CoilListClass))

#define COIL_IS_LIST_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), COIL_TYPE_LIST))

#define COIL_LIST_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), COIL_TYPE_LIST, CoilListClass))

typedef struct _CoilList CoilList;
typedef struct _CoilListClass CoilListClass;
typedef struct _CoilListPrivate CoilListPrivate;

struct _CoilList
{
    CoilObject parent_instance;
    CoilListPrivate *priv;
};

struct _CoilListClass
{
    CoilObjectClass parent_class;
};

G_BEGIN_DECLS

GType
coil_list_get_type(void) G_GNUC_CONST;

CoilObject *
coil_list_new(void);

CoilObject *
coil_list_new_sized(guint n);

guint
coil_list_length(CoilObject *list);

CoilObject *
coil_list_append(CoilObject *list, CoilValue *value);

CoilObject *
coil_list_prepend(CoilObject *list, CoilValue *value);

CoilObject *
coil_list_insert(CoilObject *list, guint index, CoilValue *value);

CoilValue *
coil_list_get_index(CoilObject *list, guint index);

CoilValue *
coil_list_dup_index(CoilObject *list, guint index);

CoilObject *
coil_list_remove_range(CoilObject *list, guint i, guint n);

G_END_DECLS

#endif
