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

#include "struct_table.h"

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
    CoilObject     parent_instance;
    CoilStructPrivate *priv;
};

struct _CoilStructClass
{
    CoilObjectClass parent_class;
};

/* XXX: move to struct.c */
struct _CoilStructIter
{
    CoilStruct *node;
    GList      *position;
#if COIL_DEBUG
    guint       version;
#endif
};

typedef gboolean (*CoilStructFunc)(CoilObject *, gpointer);

G_BEGIN_DECLS

/* XXX: make private */
gboolean
make_prototype_final(CoilStruct *self,
                     gpointer    unused);

GType
coil_struct_get_type(void) G_GNUC_CONST;

GQuark
coil_struct_prototype_quark(void) G_GNUC_CONST;

CoilObject *
coil_struct_new(GError **error, const gchar *first_property_name, ...);

CoilObject *
coil_struct_new_valist(const gchar *first_property_name,
        va_list properties, GError **error);


/* XXX: make private */
CoilObject *
coil_struct_create_containers(CoilObject *self,
                              const gchar *path, guint path_len,
                              gboolean prototype,
                              gboolean has_previous_lookup,
                              GError **error);

/* XXX: make private */
CoilObject *
coil_struct_create_containers_fast(CoilObject *self,
                                   const gchar *path,
                                   guint path_len,
                                   gboolean prototype,
                                   gboolean has_previous_lookup,
                                   GError **error);

void
coil_struct_empty(CoilObject *self, GError **error);

gboolean
coil_struct_is_root(CoilObject *self);

gboolean
coil_struct_is_prototype(CoilObject *self);

gboolean
coil_struct_is_empty(CoilObject *self, GError **error);

gboolean
coil_struct_is_ancestor(CoilObject *ancestor, CoilObject *descendent);

gboolean
coil_struct_is_descendent(CoilObject *descendent, CoilObject *ancestor);

void
coil_struct_foreach_ancestor(CoilObject *self,
                             gboolean include_self,
                             CoilStructFunc func,
                             gpointer user_data);

/* XXX: make private */
gboolean
coil_struct_insert_path(CoilObject *self,
                        CoilPath *path, /* steals */
                        GValue *value, /* steals */
                        gboolean replace,
                        GError **error);

gboolean
coil_struct_insert(CoilObject  *self,
                   gchar *path_str, /* steals */
                   guint path_len,
                   GValue *value, /* steals */
                   gboolean replace,
                   GError **error);

/* XXX: remove */
gboolean
coil_struct_insert_fast(CoilObject *self,
                        gchar *path_str, /* steals */
                        guint8 path_len,
                        GValue *value, /* steals */
                        gboolean replace,
                        GError **error);

/* XXX: remove */
gboolean
coil_struct_insert_key(CoilObject *self,
                       const gchar *key,
                       guint key_len,
                       GValue *value, /* steals */
                       gboolean replace,
                       GError **error);

/* XXX: remove */
gboolean
coil_struct_delete_path(CoilObject *self, CoilPath *path,
        gboolean strict, GError **error);

gboolean
coil_struct_delete(CoilObject *self, const gchar *path, guint path_len,
                   gboolean strict, GError **error);

/* XXX: remove */
gboolean
coil_struct_delete_key(CoilObject  *self,
                       const gchar *key, guint key_len,
                       gboolean strict, GError **error);

/* XXX: make private */
gboolean
coil_struct_mark_deleted_path(CoilObject *self,
                              CoilPath *path, /* steal */
                              gboolean force,
                              GError **error);

/* XXX: remove */
gboolean
coil_struct_mark_deleted_fast(CoilObject *self,
                              gchar *path_str, /* steal */
                              guint8 path_len,
                              gboolean force,
                              GError **error);

/* XXX: remove */
gboolean
coil_struct_mark_deleted(CoilObject *self,
                         gchar *path_str, /* steal */
                         guint path_len,
                         gboolean force,
                         GError **error);

/* XXX: remove */
gboolean
coil_struct_mark_deleted_key(CoilObject *self, const gchar *key,
        gboolean force, GError **error);

gboolean
coil_struct_add_dependency(CoilObject *self,
        CoilObject *object, GError **error);

gboolean
coil_struct_extend(CoilObject *self,
        CoilObject *parent, GError **error);

gboolean
coil_struct_extend_path(CoilObject *self,
                        CoilPath *path, /* steal */
                        CoilObject *context,
                        GError **error);

gboolean
coil_struct_extend_paths(CoilObject *self,
                         GList *path_list, /* steals */
                         CoilObject *context,
                         GError **error);

void
coil_struct_iter_init(CoilStructIter *iter, CoilObject *self);

gboolean
coil_struct_iter_next(CoilStructIter *iter, CoilPath **path,
                      const GValue **value);

/* XXX: remove */
gboolean
coil_struct_iter_next_expand(CoilStructIter *iter, CoilPath **path,
        const GValue **value, gboolean recursive, GError **error);

gboolean
coil_struct_merge_full(CoilObject *src, CoilObject *dst,
        gboolean overwrite, gboolean force_expand, GError **error);

gboolean
coil_struct_merge(CoilObject *src, CoilObject *dst, GError **error);

/* XXX: remove */
gboolean
coil_struct_expand(CoilObject *self, GError **error);

/* XXX: remove (replace with visitor pattern for object entries) */
gboolean
coil_struct_expand_items(CoilObject *self, gboolean recursive, GError **error);

/* XXX: remove */
const GValue *
coil_struct_lookup_path(CoilObject *self, CoilPath *path,
        gboolean expand_value, GError **error);

/* XXX: remove */
const GValue *
coil_struct_lookup_key(CoilObject  *self, const gchar *key, guint key_len,
        gboolean expand_value, GError **error);

/* XXX: remove */
const GValue *
coil_struct_lookup_fast(CoilObject *self, const gchar *path, guint8 len,
        gboolean expand_value, GError **error);

const GValue *
coil_struct_lookup(CoilObject *self, const gchar *path, guint len,
        gboolean expand_value, GError **error);

GList *
coil_struct_get_paths(CoilObject *self, GError **error);

GList *
coil_struct_get_values(CoilObject *self, GError **error);

GNode *
coil_struct_dependency_tree(CoilObject *self, guint n_types, ...);

GNode *
coil_struct_dependency_valist(CoilObject *self, guint ntypes, va_list args);

GNode *
coil_struct_dependency_treev(CoilObject *self, GNode *tree, guint ntypes,
        GType *allowed_types, GError **error);

gint
coil_struct_get_size(CoilObject *self, GError **error);

/* XXX: remove */
void
coil_struct_build_string(CoilObject *self,
        GString *const buffer, CoilStringFormat *format, GError **error);

/* XXX: remove */
gchar *
coil_struct_to_string(CoilObject *self,
        CoilStringFormat *format, GError **error);

/* XXX: remove */
CoilObject *
coil_struct_copy(CoilObject *self, GError **error,
        const gchar *first_property_name, ...) G_GNUC_NULL_TERMINATED;

/* XXX: remove */
CoilObject *
coil_struct_copy_valist(CoilObject *self,
        const gchar *first_property_name, va_list properties, GError **error);

/* XXX: remove */
gboolean
coil_struct_equals(CoilObject *a, CoilObject *b, GError **error);

G_END_DECLS

#endif
