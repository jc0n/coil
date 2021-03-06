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
#include "expandable.h"
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
  CoilExpandable     parent_instance;
  CoilStructPrivate *priv;
};

struct _CoilStructClass
{
  CoilExpandableClass parent_class;
};

struct _CoilStructIter
{
  CoilStruct *node;
  GList      *position;
#if COIL_DEBUG
  guint       version;
#endif
};

typedef gboolean (*CoilStructFunc)(CoilStruct *, gpointer);

G_BEGIN_DECLS

gboolean
make_prototype_final(CoilStruct *self,
                     gpointer    unused);

GType
coil_struct_get_type(void) G_GNUC_CONST;

GQuark
coil_struct_prototype_quark(void) G_GNUC_CONST;

CoilStruct *
coil_struct_new(GError **error,
                const gchar *first_property_name,
                ...);

CoilStruct *
coil_struct_new_valist(const gchar *first_property_name,
                       va_list      properties,
                       GError     **error);

CoilStruct *
coil_struct_create_containers(CoilStruct  *self,
                              const gchar *path,
                              guint        path_len,
                              gboolean     prototype,
                              gboolean     has_previous_lookup,
                              GError     **error);

CoilStruct *
coil_struct_create_containers_fast(CoilStruct  *self,
                                   const gchar *path,
                                   guint8       path_len,
                                   gboolean     prototype,
                                   gboolean     has_previous_lookup,
                                   GError     **error);

void
coil_struct_empty(CoilStruct *self,
                  GError    **error);

gboolean
coil_struct_is_root(const CoilStruct *self);

gboolean
coil_struct_is_prototype(const CoilStruct *self);

gboolean
coil_struct_is_empty(CoilStruct *self,
                     GError    **error);

gboolean
coil_struct_is_ancestor(const CoilStruct *ancestor,
                        const CoilStruct *descendent);

gboolean
coil_struct_is_descendent(const CoilStruct *descendent,
                          const CoilStruct *ancestor);

CoilStruct *
coil_struct_get_root(const CoilStruct *self);

CoilStruct *
coil_struct_get_container(const CoilStruct *self);

const CoilPath *
coil_struct_get_path(const CoilStruct *self);

gboolean
coil_struct_compare_root(const CoilStruct *a,
                          const CoilStruct *b);

void
coil_struct_foreach_ancestor(CoilStruct     *self,
                             gboolean        include_self,
                             CoilStructFunc  func,
                             gpointer        user_data);

gboolean
coil_struct_insert_path(CoilStruct *self,
                        CoilPath   *path, /* steals */
                        GValue     *value, /* steals */
                        gboolean    replace,
                        GError    **error);

gboolean
coil_struct_insert(CoilStruct  *self,
                   gchar       *path_str, /* steals */
                   guint        path_len,
                   GValue      *value, /* steals */
                   gboolean     replace,
                   GError     **error);

gboolean
coil_struct_insert_fast(CoilStruct  *self,
                        gchar       *path_str, /* steals */
                        guint8       path_len,
                        GValue      *value, /* steals */
                        gboolean     replace,
                        GError     **error);
gboolean
coil_struct_insert_key(CoilStruct   *self,
                       const gchar  *key,
                       guint         key_len,
                       GValue       *value, /* steals */
                       gboolean      replace,
                       GError      **error);

gboolean
coil_struct_delete_path(CoilStruct *self,
                        CoilPath   *path,
                        gboolean    strict,
                        GError    **error);

gboolean
coil_struct_delete(CoilStruct  *self,
                   const gchar *path,
                   guint        path_len,
                   gboolean     strict,
                   GError     **error);

gboolean
coil_struct_delete_key(CoilStruct  *self,
                       const gchar *key,
                       guint        key_len,
                       gboolean     strict,
                       GError     **error);

gboolean
coil_struct_mark_deleted_path(CoilStruct *self,
                              CoilPath   *path, /* steal */
                              gboolean    force,
                              GError    **error);

gboolean
coil_struct_mark_deleted_fast(CoilStruct  *self,
                              gchar       *path_str, /* steal */
                              guint8       path_len,
                              gboolean     force,
                              GError     **error);

gboolean
coil_struct_mark_deleted(CoilStruct  *self,
                         gchar       *path_str, /* steal */
                         guint        path_len,
                         gboolean     force,
                         GError     **error);

gboolean
coil_struct_mark_deleted_key(CoilStruct  *self,
                             const gchar *key,
                             gboolean     force,
                             GError     **error);

gboolean
coil_struct_add_dependency(CoilStruct     *self,
                           gpointer        object,
                           GError        **error);

gboolean
coil_struct_extend(CoilStruct  *self,
                   CoilStruct  *parent,
                   GError     **error);

gboolean
coil_struct_extend_path(CoilStruct  *self,
                        CoilPath    *path, /* steal */
                        CoilStruct  *context,
                        GError     **error);

gboolean
coil_struct_extend_paths(CoilStruct *self,
                         GList      *path_list, /* steals */
                         CoilStruct *context,
                         GError    **error);

void
coil_struct_iter_init(CoilStructIter *iter,
                      CoilStruct     *self);

gboolean
coil_struct_iter_next(CoilStructIter  *iter,
                      const CoilPath **path,
                      const GValue   **value);

gboolean
coil_struct_iter_next_expand(CoilStructIter  *iter,
                             const CoilPath **path,
                             const GValue   **value,
                             gboolean         recursive,
                             GError         **error);

gboolean
coil_struct_merge_full(CoilStruct  *src,
                       CoilStruct  *dst,
                       gboolean     overwrite,
                       gboolean     force_expand,
                       GError     **error);

gboolean
coil_struct_merge(CoilStruct *src,
                  CoilStruct *dst,
                  GError    **error);

gboolean
coil_struct_expand(CoilStruct *self,
                   GError    **error);

gboolean
coil_struct_expand_items(CoilStruct  *self,
                         gboolean     recursive,
                         GError     **error);

const GValue *
coil_struct_lookup_path(CoilStruct *self,
                        CoilPath   *path,
                        gboolean    expand_value,
                        GError    **error);

const GValue *
coil_struct_lookup_key(CoilStruct  *self,
                       const gchar *key,
                       guint        key_len,
                       gboolean     expand_value,
                       GError     **error);

const GValue *
coil_struct_lookup_fast(CoilStruct  *self,
                        const gchar *path_str,
                        guint8       path_len,
                        gboolean     expand_value,
                        GError     **error);

const GValue *
coil_struct_lookup(CoilStruct  *self,
                   const gchar *path_str,
                   guint        path_len,
                   gboolean     expand_value,
                   GError     **error);

GList *
coil_struct_get_paths(CoilStruct *self,
                      GError    **error);

GList *
coil_struct_get_values(CoilStruct *self,
                       GError    **error);

GNode *
coil_struct_dependency_tree(CoilStruct *self,
                            guint       n_types,
                            ...);

GNode *
coil_struct_dependency_valist(CoilStruct *self,
                              guint       ntypes,
                              va_list     args);

GNode *
coil_struct_dependency_treev(CoilStruct *self,
                             GNode      *tree,
                             guint       ntypes,
                             GType      *allowed_types,
                             GError    **error);

gint
coil_struct_get_size(CoilStruct *self,
                     GError    **error);

void
coil_struct_build_string(CoilStruct       *self,
                         GString          *const buffer,
                         CoilStringFormat *format,
                         GError          **error);

gchar *
coil_struct_to_string(CoilStruct       *self,
                      CoilStringFormat *format,
                      GError          **error);

CoilStruct *
coil_struct_copy(CoilStruct       *self,
                 GError          **error,
                 const gchar      *first_property_name,
                 ...) G_GNUC_NULL_TERMINATED;

CoilStruct *
coil_struct_copy_valist(CoilStruct  *self,
                        const gchar *first_property_name,
                        va_list      properties,
                        GError     **error);
gboolean
coil_struct_equals(gconstpointer e1,
                   gconstpointer e2,
                   GError        **error);

G_END_DECLS

#endif
