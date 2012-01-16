/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */
#ifndef __COIL_STRUCT_H
#define __COIL_STRUCT_H

typedef struct _CoilStruct        CoilStruct;
typedef struct _CoilStructClass   CoilStructClass;
typedef struct _CoilStructPrivate CoilStructPrivate;
typedef struct _CoilStructIter    CoilStructIter;

#include "path.h"
#include "object.h"
#include "value.h"

#define COIL_TYPE_STRUCT              \
        (coil_struct_get_type())

#define COIL_STRUCT(obj)              \
        (G_TYPE_CHECK_INSTANCE_CAST((obj), COIL_TYPE_STRUCT, CoilStruct))

#define COIL_IS_STRUCT(obj)           \
        (G_TYPE_CHECK_INSTANCE_TYPE((obj), COIL_TYPE_STRUCT))

#define COIL_STRUCT_CLASS(klass)      \
        (G_TYPE_CHECK_CLASS_CAST((klass), COIL_TYPE_STRUCT, CoilStructClass))

#define COIL_IS_STRUCT_CLASS(klass)   \
        (G_TYPE_CHECK_CLASS_TYPE((klass), COIL_TYPE_STRUCT))

#define COIL_STRUCT_GET_CLASS(obj)    \
        (G_TYPE_INSTANCE_GET_CLASS((obj), COIL_TYPE_STRUCT, CoilStructClass))

struct _CoilStruct
{
    CoilObject parent_instance;
    CoilStructPrivate *priv;
};

struct _CoilStructClass
{
    CoilObjectClass parent_class;
};

struct _CoilStructIter
{
    CoilObject *node;
    GList      *position;
#if COIL_DEBUG
    guint       version;
#endif
    gboolean    reversed : 1;
};

typedef gboolean (*CoilStructFunc)(CoilObject *, gpointer);

G_BEGIN_DECLS

GType
coil_struct_get_type(void) G_GNUC_CONST;

GQuark
coil_struct_prototype_quark(void) G_GNUC_CONST;

CoilObject *
coil_struct_new(const gchar *first_property_name, ...);

CoilObject *
coil_struct_new_valist(const gchar *first_property_name, va_list properties);

void
coil_struct_empty(CoilObject *self);

gboolean
coil_struct_is_root(CoilObject *self);

gboolean
coil_struct_is_prototype(CoilObject *self);

gboolean
coil_struct_is_empty(CoilObject *self);

gboolean
coil_struct_is_ancestor(CoilObject *ancestor, CoilObject *descendent);

gboolean
coil_struct_is_descendent(CoilObject *descendent, CoilObject *ancestor);

/* TODO: replace with generic traverse routine */
void
coil_struct_foreach_ancestor(CoilObject *self,
                             gboolean include_self,
                             CoilStructFunc func,
                             gpointer user_data);

gboolean
coil_struct_insert_path(CoilObject *self, CoilPath *path,
                        GValue *value, /* steals */
                        gboolean replace);

gboolean
coil_struct_insert(CoilObject  *self, const gchar *str, guint len,
                   GValue *value, /* steals */
                   gboolean replace);


gboolean
coil_struct_delete(CoilObject *self, const gchar *str, guint len,
                   gboolean strict);


gboolean
coil_struct_delete_path(CoilObject *self, CoilPath *path, gboolean strict);

void
coil_struct_iter_init(CoilStructIter *iter, CoilObject *self);

gboolean
coil_struct_iter_next(CoilStructIter *iter, CoilPath **path,
                      const GValue **value);

gboolean
coil_struct_merge_full(CoilObject *src, CoilObject *dst,
        gboolean overwrite, gboolean force_expand);

gboolean
coil_struct_merge(CoilObject *src, CoilObject *dst);

/* XXX: remove (replace with visitor pattern for object entries) */
gboolean
coil_struct_expand_items(CoilObject *self, gboolean recursive);

/* XXX: maybe rename this back to lookup_path,
 * either way it needs to maintain the same scheme as the others */
const GValue *
coil_struct_lookupx(CoilObject *self, CoilPath *path,
        gboolean expand_value);

/* XXX: remove */
const GValue *
coil_struct_lookup(CoilObject *self, const gchar *str, guint len,
        gboolean expand_value);

GList *
coil_struct_get_paths(CoilObject *self);

GList *
coil_struct_get_values(CoilObject *self);

GNode *
coil_struct_dependency_tree(CoilObject *self, guint n_types, ...);

GNode *
coil_struct_dependency_valist(CoilObject *self, guint ntypes, va_list args);

GNode *
coil_struct_dependency_treev(CoilObject *self, GNode *tree, guint ntypes,
        GType *allowed_types);

gint
coil_struct_get_size(CoilObject *self);

G_END_DECLS

#endif
