/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */
#ifndef COIL_LINK_H
#define COIL_LINK_H

#define COIL_TYPE_LINK              \
        (coil_link_get_type())

#define COIL_LINK(obj)              \
        (G_TYPE_CHECK_INSTANCE_CAST((obj), COIL_TYPE_LINK, CoilLink))

#define COIL_IS_LINK(obj)           \
        (G_TYPE_CHECK_INSTANCE_TYPE((obj), COIL_TYPE_LINK))

#define COIL_LINK_CLASS(klass)      \
        (G_TYPE_CHECK_CLASS_CAST((klass), COIL_TYPE_LINK, CoilLinkClass))

#define COIL_IS_LINK_CLASS(klass)   \
        (G_TYPE_CHECK_CLASS_TYPE((klass), COIL_TYPE_LINK))

#define COIL_LINK_GET_CLASS(klass)  \
        (G_TYPE_INSTANCE_GET_CLASS((obj), COIL_TYPE_LINK, CoilLinkClass))


typedef struct _CoilLink        CoilLink;
typedef struct _CoilLinkClass   CoilLinkClass;
typedef struct _CoilLinkPrivate CoilLinkPrivate;


struct _CoilLink
{
  CoilExpandable   parent_instance;
  CoilLinkPrivate *priv;

  /* public */
  CoilPath *target_path;
};

struct _CoilLinkClass
{
  CoilExpandableClass parent_class;
};

G_BEGIN_DECLS

GType coil_link_get_type(void) G_GNUC_CONST;

CoilLink *
coil_link_new(const gchar *first_property_name, ...) G_GNUC_WARN_UNUSED_RESULT
                                                     G_GNUC_NULL_TERMINATED;

const CoilPath *
coil_link_get_path(const CoilLink *link);


gboolean
coil_link_equals(gconstpointer self,
                 gconstpointer other,
                 GError       **error);

void
coil_link_build_string(CoilLink *self,
                       GString  *const buffer);

gchar *
coil_link_to_string(const CoilLink *self);


G_END_DECLS

#endif

