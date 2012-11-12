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

/* XXX: make private */
CoilExpr *
coil_expr_new(GString *expr_string, const gchar *first_property_name, ...);

/* XXX: make private */
COIL_API(CoilExpr *)
coil_expr_new_string(const gchar *string, size_t len,
                     const gchar *first_property_name,
                     ...);

/* XXX: make private */
CoilExpr *
coil_expr_new_valist(GString *expr_string, const gchar *first_property_name,
        va_list properties);

G_END_DECLS

#endif
