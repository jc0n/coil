/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#ifndef COIL_EXPR_H
#define COIL_EXPR_H

#define COIL_TYPE_EXPR \
  (coil_expr_get_type())

#define COIL_EXPR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), COIL_TYPE_EXPR, CoilExpr))

#define COIL_IS_EXPR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), COIL_TYPE_EXPR))

#define COIL_EXPR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), COIL_TYPE_EXPR))

#define COIL_EXPR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj), COIL_TYPE_EXPR, CoilExprClass))

typedef struct _CoilExpr        CoilExpr;
typedef struct _CoilExprClass   CoilExprClass;
typedef struct _CoilExprPrivate CoilExprPrivate;

struct _CoilExpr
{
  CoilObject   parent_instance;
  CoilExprPrivate *priv;
};

struct _CoilExprClass
{
  CoilObjectClass parent_class;
};

G_BEGIN_DECLS

GType coil_expr_get_type(void) G_GNUC_CONST;

gchar *
coil_expr_to_string(CoilExpr         *self,
                    CoilStringFormat *format,
                    GError          **error);

CoilExpr *
coil_expr_new(GString *expr_string,
              const gchar *first_property_name,
              ...);

CoilExpr *
coil_expr_new_valist(GString *expr_string,
                     const gchar *first_property_name,
                     va_list      properties);


G_END_DECLS

#endif
