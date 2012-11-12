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

#define COIL_LINK_GET_CLASS(obj)  \
        (G_TYPE_INSTANCE_GET_CLASS((obj), COIL_TYPE_LINK, CoilLinkClass))


typedef struct _CoilLink        CoilLink;
typedef struct _CoilLinkClass   CoilLinkClass;
typedef struct _CoilLinkPrivate CoilLinkPrivate;

struct _CoilLink
{
    CoilObject   parent_instance;
    CoilLinkPrivate *priv;
};

struct _CoilLinkClass
{
    CoilObjectClass parent_class;
};

G_BEGIN_DECLS

GType coil_link_get_type(void) G_GNUC_CONST;

/* XXX: make private */
CoilObject *
coil_link_new(CoilPath *target, const gchar *first_property_name, ...)
    G_GNUC_WARN_UNUSED_RESULT G_GNUC_NULL_TERMINATED;

/* XXX: make private */
CoilObject *
coil_link_new_valist(CoilPath *target, const gchar *first_property_name,
        va_list properties) G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS
#endif

