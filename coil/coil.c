/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */
#include "common.h"
#include "value.h"

/* TODO(jcon): namespace */
CoilStringFormat default_string_format = {
    ( LEGACY
      | BLANK_LINE_AFTER_BRACE
      | BLANK_LINE_AFTER_STRUCT
      | ESCAPE_QUOTES
    ),
    4, /* block indent */
    0,  /* brace indent */
    78, /* multiline len */
    0, /* indent level */
    (CoilObject *)NULL, /* context */
};

/*
 * coil_init:
 *
 * Call this before using coil. Initializes the type system
 * and the coil none type.
 */
void
coil_init(void)
{
    static gboolean init_called = FALSE;
    g_assert(init_called == FALSE);

    g_type_init();
    //  g_type_init_with_debug_flags(G_TYPE_DEBUG_SIGNALS);

    coil_none_object = g_object_new(COIL_TYPE_NONE, NULL);
    coil_path_ref(CoilRootPath);
    init_called = TRUE;
}

/*
 * coil_location_get_type:
 *
 * Get the type identifier for #CoilLocation
 */
GType
coil_location_get_type(void)
{
    static GType type_id = 0;

    if (!type_id)
        type_id = g_pointer_type_register_static("CoilLocation");

    return type_id;
}
