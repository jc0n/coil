/*
 * Author: John O'Connor
 * Copyright 2011
 */

#ifndef _COIL_STRUCT_PRIVATE
#define _COIL_STRUCT_PRIVATE

void
coil_struct_set_prototype(CoilObject *self, gboolean prototype);

void
coil_struct_set_accumulate(CoilObject *self, gboolean accumulate);

gboolean
finalize_prototype(CoilObject *self, gpointer unused);


CoilObject *
_coil_create_containers(CoilObject *self, CoilPath *path,
        gboolean prototype, gboolean has_lookup, GError **error);

gboolean
coil_struct_mark_deleted_path(CoilObject *self, CoilPath *path,
        gboolean force, GError **error);

gboolean
coil_struct_add_dependency(CoilObject *self,
        CoilObject *object, GError **error);

gboolean
coil_struct_extend_path(CoilObject *self, CoilPath *path,
        CoilObject *context, GError **error);

gboolean
coil_struct_extend_paths(CoilObject *self, GList *list,
        CoilObject *context, GError **error);

#endif
