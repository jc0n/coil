%{
/*
 * Copyright (C) 2009, 2010
 *
 * Author: John O'Connor
 */


#include "common.h"

#include <stdio.h>

#include "path.h"
#include "list.h"
#include "struct.h"
#include "struct-private.h"
#include "include.h"
#include "link.h"
#include "expression.h"

#include "parser_defs.h"
#include "parser.h"
#include "scanner.h"

#define PEEK_CONTAINER(parser) \
  COIL_OBJECT(g_queue_peek_head(&(parser)->containers))

#define POP_CONTAINER(parser) \
  COIL_OBJECT(g_queue_pop_head(&(parser)->containers))

#define PUSH_CONTAINER(parser, c) \
  g_queue_push_head(&(parser)->containers, (c))

void
yyerror(YYLTYPE *yylocp, CoilParser *parser, const gchar *msg);

static void
set_parse_error(CoilParser *parser, CoilLocation *locp, const char *format, ...)
{
    g_return_if_fail(parser);

    CoilError *error;
    va_list args;

    va_start(args, format);
    error = coil_error_new_valist(COIL_ERROR_PARSE, locp, format, args);
    va_end(args);

    parser->errors = g_list_prepend(parser->errors, error);
}

static void
handle_internal_error(CoilParser *parser)
{
    g_return_if_fail(parser);

    CoilError *error = NULL;

    if (coil_get_error(&error)) {
        CoilError *copy = coil_error_copy(error);
        parser->errors = g_list_prepend(parser->errors, copy);
        coil_error_clear();
    }
}

static char _parse_errors[] = "Parse Errors: \n";
static char _parse_error[] = "Parse Error: ";

static void
collect_parse_errors(CoilParser *parser)
{
    g_return_if_fail(parser);

    guint i, n;
    GList *list;
    GString *msg;
    CoilError *error;
    gboolean show_numbers;

    n = g_list_length(parser->errors);
    if (n == 0) {
        /* no errors to collect */
        return;
    }
    if (n > 1) {
        show_numbers = TRUE;
        msg = g_string_new_len(_parse_errors, sizeof(_parse_errors)-1);
    }
    else {
        show_numbers = FALSE;
        msg = g_string_new_len(_parse_error, sizeof(_parse_error)-1);
    }

    list = g_list_last(parser->errors);
    for (i = 1; list != NULL; i++) {
        error = (CoilError *)list->data;
        if (show_numbers) {
            g_string_append_printf(msg, "%d) %s\n", i, error->message);
        }
        else {
            g_string_append(msg, error->message);
        }
        g_string_append_c(msg, '\n');
        list = g_list_previous(list);
    }
    coil_set_error(COIL_ERROR_PARSE, NULL, msg->str);
    g_string_free(msg, TRUE);
}

static gboolean
handle_undefined_prototypes(CoilParser *parser)
{
    g_return_val_if_fail(parser != NULL, FALSE);

    GList *prototypes;
    GHashTable *proto_table = parser->prototypes;

    if (g_hash_table_size(proto_table) == 0)
        return TRUE;

    /* Attempt necessary expansions to finalize prototypes which are
    defined after an expansion */
    prototypes = g_hash_table_get_values(proto_table);
    while (prototypes != NULL) {
        CoilObject *prototype = COIL_OBJECT(prototypes->data);

        if (!coil_struct_is_prototype(prototype->container)) {
            coil_object_expand(prototype->container, NULL, FALSE);
        }
        if (coil_error_occurred()) {
            handle_internal_error(parser);
            g_list_free(prototypes);
            return FALSE;
        }
        if (coil_struct_is_prototype(prototype)) {
            g_list_free(prototypes);
            break;
        }
        prototypes = g_list_delete_link(prototypes, prototypes);
    }
    /* If we still have undefined prototypes this is an error */
    if (g_hash_table_size(proto_table) > 0) {
        GList   *list, *lp;
        GString *msg = g_string_sized_new(512);

        g_string_append(msg, "Referencing undefined structs, prototypes are: ");
        list = g_hash_table_get_values(proto_table);

        /* convert list of prototypes to list of paths */
        for (lp = list; lp; lp = g_list_next(lp)) {
            CoilObject *prototype = COIL_OBJECT(lp->data);

            lp->data = prototype->path;
            /* cast the prototype to an empty struct
               to allow continue despite errors */
            coil_struct_foreach_ancestor(prototype, TRUE,
                (CoilStructFunc)finalize_prototype, NULL);
        }

        /* sort by path and build error message */
        list = g_list_sort(list, (GCompareFunc)coil_path_compare);
        while (list != NULL) {
            const CoilPath *path = (CoilPath *)list->data;
            g_string_append_printf(msg, "%s, ", path->str);
            list = g_list_delete_link(list, list);
        }
        set_parse_error(parser, NULL, "%s", msg->str);
        g_string_free(msg, TRUE);
        return FALSE;
    }
    return TRUE;
}

static gboolean
parser_post_processing(CoilParser *parser)
{
    g_return_val_if_fail(parser != NULL, FALSE);

    if (!handle_undefined_prototypes(parser))
        return FALSE;

    return TRUE;
}

static gboolean
parser_push_container(CoilParser *parser)
{
    CoilObject *new, *container = PEEK_CONTAINER(parser);

    new = _coil_create_containers(container, parser->path, FALSE, FALSE);
    if (new == NULL) {
        return FALSE;
    }
    coil_struct_set_accumulate(new, TRUE);
    PUSH_CONTAINER(parser, new);
    return TRUE;
}

static void
parser_pop_container(CoilParser *parser)
{
    CoilObject *container = POP_CONTAINER(parser);

    coil_struct_set_accumulate(container, FALSE);
    coil_object_unref(container);
}

/* arguments can take the following form..
    case 1. @include: <single value>     (parsed as the filename)
    - or -
    case 2. @include: [filename [paths...]]
    - or -
    case 3. @include: [[filename [paths...]]...]
*/
static gboolean
parser_handle_include(CoilParser *parser, CoilLocation *location,
    GList *include_args)
{
    g_return_val_if_fail(parser, FALSE);
    g_return_val_if_fail(location, FALSE);

    CoilObject *include;
    CoilObject *container = PEEK_CONTAINER(parser);
    GValue *file_value, *import;
    GValueArray *imports;
    gsize i, n;

    if (include_args == NULL) {
        set_parse_error(parser, location,
            "No filename specified for file include.");
        return FALSE;
    }

    file_value = (GValue *)include_args->data;
    include_args = g_list_delete_link(include_args, include_args);

    n = g_list_length(include_args);
    imports = g_value_array_new(n);

    for (i = 0; i < n; i++) {
        import = (GValue *)include_args->data;
        imports = g_value_array_insert(imports, i, import);
        include_args = g_list_delete_link(include_args, include_args);
    }

    g_assert(container != NULL);

    include = coil_include_new("file_value", file_value,
                               "imports", imports,
                               "container", container,
                               "location", location,
                               NULL);
    if (include == NULL) {
        return FALSE;
    }

    coil_struct_add_dependency(container, include);
    coil_object_unref(include);
    if (coil_error_occurred()) {
        return FALSE;
    }
    return TRUE;
}

static CoilPath *
parser_make_path_from_string(CoilParser *parser, GString *gstring)
{
    g_return_val_if_fail(parser, NULL);
    g_return_val_if_fail(gstring, NULL);

    CoilPath *path;

    if (!coil_check_path(gstring->str, gstring->len)) {
        g_string_free(gstring, TRUE);
        return NULL;
    }
    path = coil_path_take_string(gstring->str, gstring->len);
    g_string_free(gstring, FALSE);
    return path;
}
%}
%expect 1
%error-verbose
%defines
%locations
%require "2.0"
%pure_parser
%parse-param { CoilParser *yyctx }

%token ERROR

%token EXTEND_SYM
%token INCLUDE_SYM
%token MODULE_SYM
%token PACKAGE_SYM

%token ABSOLUTE_PATH
%token RELATIVE_PATH
%token REFERENCE_PATH
%token ROOT_PATH

%token TRUE_SYM
%token FALSE_SYM
%token NONE_SYM

%token DOUBLE
%token INTEGER

%token STRING_LITERAL
%token STRING_EXPRESSION

%token UNKNOWN_SYM

%left ':' ','
%right '~' '@' '='

%union {
    CoilPath    *path;
    GValue      *value;
    glong        longint;
    gdouble      doubleval;
    GList       *path_list;
    GList       *value_list;
    GValueArray *value_array;
    GString     *gstring;
}

%type <path> ABSOLUTE_PATH
%type <path> REFERENCE_PATH
%type <path> RELATIVE_PATH
%type <path> ROOT_PATH
%type <path> path
%type <path> pathstring

%type <path_list> path_list
%type <path_list> path_list_comma
%type <path_list> path_list_comma_items
%type <path_list> path_list_spaced

%type <value_array> value_list
%type <value_list> value_list_contents
%type <value_list> value_list_items
%type <value_list> value_list_items_comma

%type <value> include_import_path
%type <value_list> include_arglist
%type <value_list> include_args
%type <value_list> include_file
%type <value_list> include_import_list
%type <value_list> include_import_list_comma
%type <value_list> include_import_list_spaced

%type <doubleval> DOUBLE
%type <longint> INTEGER

%type <gstring> STRING_EXPRESSION
%type <gstring> STRING_LITERAL

%type <value> link
%type <value> link_path
%type <value> pathstring_value
%type <value> primative
%type <value> string
%type <value> value

%destructor { g_free($$); } <string>
%destructor { g_string_free($$, TRUE); } <gstring>
%destructor { coil_path_unref($$); } <path>
%destructor { coil_path_list_free($$); } <path_list>
%destructor { coil_value_free($$); } <value>
%destructor { coil_value_list_free($$); } <value_list>
%destructor { g_value_array_free($$); } <value_array>

%start coil

%%

coil
    : context
;

context
    : /* empty */
    | context statement
    |  error {
        CoilObject *container = PEEK_CONTAINER(YYCTX);

        if (!coil_struct_is_root(container)) {
            parser_pop_container(YYCTX);
        }
        handle_internal_error(YYCTX);
    }
;

statement
    : builtin_property
    | deletion
    | assignment
;

deletion
    : '~' RELATIVE_PATH {
        CoilObject *container = PEEK_CONTAINER(YYCTX);

        if (!coil_struct_mark_deleted_path(container, $2, FALSE)) {
            coil_path_unref($2);
            YYERROR;
        }
        coil_path_unref($2);
    }
;

assignment
    : assignment_path ':' assignment_value
;

assignment_path
    : RELATIVE_PATH {
        CoilObject *container = PEEK_CONTAINER(YYCTX);

        if (YYCTX->path) {
            coil_path_unref(YYCTX->path);
            YYCTX->path = NULL;
        }

        /* resolve path to prevent doing this more than once in other places */
        YYCTX->path = coil_path_resolve($1, container->path);
        coil_path_unref($1);

        if (coil_error_occurred()) {
            YYERROR;
        }
    }
;

assignment_value
    : container
    | value {
        CoilObject *container = PEEK_CONTAINER(YYCTX);

        if (!coil_struct_insert_path(container, YYCTX->path, $1, FALSE)) {
            coil_path_unref(YYCTX->path);
            YYCTX->path = NULL;
            YYERROR;
        }
        coil_path_unref(YYCTX->path);
        YYCTX->path = NULL;
    }
;

container
    : container_declaration {
        CoilObject *container = PEEK_CONTAINER(YYCTX);

        if (!coil_struct_is_root(container)) {
            parser_pop_container(YYCTX);
        }
    }
;

container_declaration
    : container_context_inherit
    | container_context
;

container_context_inherit
    : path_list_comma '{' {
        CoilObject *context = PEEK_CONTAINER(YYCTX), *container;

        if (!parser_push_container(YYCTX)) {
            YYERROR;
        }
        container = PEEK_CONTAINER(YYCTX);
        if (!coil_struct_extend_paths(container, $1, context)) {
            YYERROR;
        }
        coil_path_list_free($1);
    } context '}'
;

container_context
    : '{' {
        if (!parser_push_container(YYCTX)) {
            YYERROR;
        }
    } context '}'
;

builtin_property
    : extend_property
    | include_property
;

extend_property_declaration
    : EXTEND_SYM
    | EXTEND_SYM ':' /* compat */
;

extend_property
    : extend_property_declaration path_list {
        CoilObject *container = PEEK_CONTAINER(YYCTX);

        if (!coil_struct_extend_paths(container, $2, NULL)) {
            YYERROR;
        }
        coil_path_list_free($2);
    }
;

include_declaration
    : INCLUDE_SYM
    | INCLUDE_SYM ':' /* compat */
;

include_property
    : include_declaration include_args {
        if (!parser_handle_include(YYCTX, &@$, $2)) {
            YYERROR;
        }
    }
;

include_args
    : include_file { $$ = $1; }
    | include_arglist { $$ = $1; }
;

include_file
    : link { $$ = g_list_prepend(NULL, $1); }
    | string { $$ = g_list_prepend(NULL, $1); }
;

include_arglist
    : '[' include_file include_import_list ']' { $$ = g_list_concat($2, $3); }
;

include_import_list
    : /* empty */ { $$ = NULL; }
    | include_import_list_spaced { $$ = $1; }
    | ',' include_import_list_comma { $$ = $2; }
    | ',' include_import_list_comma ',' { $$ = $2; }
;

include_import_list_spaced
    : include_import_path { $$ = g_list_prepend(NULL, $1); }
    | include_import_list_spaced include_import_path {
        $$ = g_list_prepend($1, $2);
    }
;

include_import_list_comma
    : include_import_path { $$ = g_list_prepend(NULL, $1); }
    | include_import_list_comma ',' include_import_path {
        $$ = g_list_prepend($1, $3);
    }
;

include_import_path
    : link { $$ = $1; }
    | pathstring_value { $$ = $1; }
;

link
    : link_path { $$ = $1; }
    | '=' link_path { $$ = $2; }
;

link_path
    : path {
        /* XXX: YYCTX->path is null when link is not assigned to a path
          ie. @extends: [ =..some_node ]
        */
        CoilObject *link = coil_link_new($1,
            "path", YYCTX->path,
            "container", PEEK_CONTAINER(YYCTX),
            "location", &@$,
            NULL);

        coil_path_unref($1);
        if (link == NULL) {
            YYERROR;
        }
        coil_value_init($$, COIL_TYPE_LINK, take_object, link);
    }
;

pathstring_value
    : pathstring { coil_value_init($$, COIL_TYPE_PATH, take_boxed, $1); }
;

pathstring
    : STRING_LITERAL {
        if (!($$ = parser_make_path_from_string(YYCTX, $1))) {
            YYERROR;
        }
    }
;

path_list
      : '[' path_list_spaced ']' { $$ = $2; }
      | path_list_comma { $$ = $1; } /* TODO(jcon): examine shift reduce conflict */
;

path_list_spaced
    : path { $$ = g_list_prepend(NULL, $1); }
    | path_list_spaced path { $$ = g_list_prepend($1, $2); }
;

path_list_comma
    : path_list_comma_items { $$ = $1; }
    | path_list_comma_items ',' { $$ = $1; }
;

path_list_comma_items
    : path { $$ = g_list_prepend(NULL, $1); }
    | path_list_comma_items ',' path { $$ = g_list_prepend($1, $3); }
;

value_list
    : '[' ']' { $$ = g_value_array_new(0); }
    | '[' value_list_contents ']' {
        /* value list is backed with a g_value_array
        XXX: this may change soon */
        gsize i, n = g_list_length($2);
        GList *list = g_list_last($2);

        $$ = g_value_array_new(n);
        /* list is reversed so restore order here */
        for (i = 0; i < n; i++) {
            GValue *value = (GValue *)list->data;
            g_value_array_insert($$, i, value);
            list = g_list_previous(list);
        }
    }
;

value_list_contents
    : value_list_items_comma { $$ = $1; }
    | value_list_items_comma value { $$ = g_list_prepend($1, $2); }
    | value_list_items { $$ = $1; }
;

value_list_items
    : value { $$ = g_list_prepend(NULL, $1); }
    | value_list_items value { $$ = g_list_prepend($1, $2); }
;

value_list_items_comma
    : value ',' { $$ = g_list_prepend(NULL, $1); }
    | value_list_items_comma value ',' { $$ = g_list_prepend($1, $2); }
;

value
    : primative { $$ = $1; }
    | string { $$ = $1; }
    | link { $$ = $1; }
    | value_list { coil_value_init($$, COIL_TYPE_LIST, take_boxed, $1); }
;

path
    : ABSOLUTE_PATH { $$ = $1; }
    | REFERENCE_PATH { $$ = $1; }
    | RELATIVE_PATH { $$ = $1; }
    | ROOT_PATH { $$ = $1; }
;

string
    : STRING_LITERAL {
        coil_value_init($$, G_TYPE_GSTRING, take_boxed, $1);
    }
    | STRING_EXPRESSION {
        coil_value_init($$, COIL_TYPE_EXPR, take_object, coil_expr_new($1, NULL));
    }
;

primative
    : NONE_SYM {
        coil_value_init($$, COIL_TYPE_NONE, set_object, coil_none_object);
    }
    | TRUE_SYM {
        coil_value_init($$, G_TYPE_BOOLEAN, set_boolean, TRUE);
    }
    | FALSE_SYM {
        coil_value_init($$, G_TYPE_BOOLEAN, set_boolean, FALSE);
    }
    | INTEGER {
        coil_value_init($$, G_TYPE_LONG, set_long, $1);
    }
    | DOUBLE {
        coil_value_init($$, G_TYPE_DOUBLE, set_double, $1);
    }
;

%%
extern int yydebug;

typedef struct _ParserNotify
{
    gulong handler_id;
    CoilParser *parser;
} ParserNotify;

void parser_notify_free(gpointer data, GClosure *closure)
{
    g_return_if_fail(data);
    g_free(data);
}

static void
untrack_prototype(GObject *instance, GParamSpec *unused, gpointer data)
{
    g_return_if_fail(COIL_IS_STRUCT(instance));
    g_return_if_fail(data);

    ParserNotify *notify = (ParserNotify *)data;
    CoilParser *parser = (CoilParser *)notify->parser;
    CoilObject *prototype = COIL_OBJECT(instance);

    g_return_if_fail(parser);
    g_return_if_fail(prototype);

    if (prototype->root == parser->root &&
        !coil_struct_is_prototype(prototype)) {
        g_hash_table_remove(parser->prototypes, prototype);
        g_signal_handler_disconnect(instance, notify->handler_id);
    }
}

/*
 * Called when prototype struct is constructed.
 */
static gboolean
track_prototype(GSignalInvocationHint *ihint,
                guint n_param_values,
                const GValue *param_values,
                gpointer data)
{
    g_return_val_if_fail(data, FALSE);
    g_return_val_if_fail(n_param_values > 0, FALSE);
    g_return_val_if_fail(G_VALUE_HOLDS(&param_values[0], COIL_TYPE_STRUCT), FALSE);

    CoilObject *prototype;
    CoilParser *parser = (CoilParser *)data;

    prototype = COIL_OBJECT(g_value_get_object(&param_values[0]));
    if (prototype->root == parser->root) {
        ParserNotify *notify;

        g_object_ref(prototype);
        g_hash_table_insert(parser->prototypes, prototype, prototype);

        notify = g_new(ParserNotify, 1);
        notify->parser = parser;
        notify->handler_id = g_signal_connect_data(prototype,
            "notify::is-prototype",
            G_CALLBACK(untrack_prototype),
            notify, parser_notify_free, 0);
    }
    return TRUE;
}

static void
coil_parser_init(CoilParser *parser, gpointer scanner)
{
    g_return_if_fail(parser != NULL);
    g_return_if_fail(scanner != NULL);

    memset(parser, 0, sizeof(*parser));
    if (yylex_init_extra(parser, &scanner)) {
        g_error("Could not set parser context for scanner. %s",
            g_strerror(errno));
        return;
    }

    parser->prototypes = g_hash_table_new_full(g_direct_hash, g_direct_equal,
                                               NULL, g_object_unref);

    parser->root = coil_struct_new(NULL, NULL);
    parser->scanner = scanner;

    g_object_ref(parser->root);
    g_queue_push_head(&parser->containers, parser->root);

    parser->prototype_hook_id =
        g_signal_add_emission_hook(g_signal_lookup("create", COIL_TYPE_STRUCT),
                                   coil_struct_prototype_quark(),
                                   (GSignalEmissionHook)track_prototype,
                                   (gpointer)parser, NULL);
}

static CoilObject *
coil_parser_finish(CoilParser *parser)
{
    g_return_val_if_fail(parser, NULL);

    CoilObject *container;

    parser_post_processing(parser);

    g_signal_remove_emission_hook(g_signal_lookup("create", COIL_TYPE_STRUCT),
                                  parser->prototype_hook_id);

    g_hash_table_destroy(parser->prototypes);

    while ((container = g_queue_pop_head(&parser->containers))) {
        coil_object_unref(container);
    }
    if (parser->path) {
        coil_path_unref(parser->path);
    }
    if (parser->do_buffer_gc) {
        yy_delete_buffer((YY_BUFFER_STATE)parser->buffer_state,
                         (yyscan_t)parser->scanner);
    }
    if (parser->scanner) {
        yylex_destroy((yyscan_t)parser->scanner);
    }
    if (parser->errors != NULL) {
        collect_parse_errors(parser);
        while (parser->errors) {
            coil_error_free((CoilError *)parser->errors->data);
            parser->errors = g_list_delete_link(parser->errors, parser->errors);
        }
    }
    return parser->root;
}

static void
coil_parser_prepare_for_stream(CoilParser *const parser, FILE *stream,
                               const gchar *name)
{
    g_return_if_fail(parser != NULL);
    g_return_if_fail(stream != NULL);
    g_return_if_fail(name != NULL);

    yyset_in(stream, (yyscan_t)parser->scanner);
    parser->filepath = name;
}

static void
coil_parser_prepare_for_string(CoilParser *const parser,
                               const char *string, gsize len)
{
    g_return_if_fail(parser != NULL);
    g_return_if_fail(string != NULL);
    g_return_if_fail(parser->buffer_state == NULL);

    if (len > 0) {
        parser->buffer_state = (gpointer)yy_scan_bytes(string,
            (yy_size_t)len, (yyscan_t)parser->scanner);
    }
    else {
        parser->buffer_state = (gpointer)yy_scan_string(string,
            (yyscan_t)parser->scanner);
    }
    if (parser->buffer_state == NULL) {
        g_error("Error preparing buffer for scanner.");
    }
    parser->do_buffer_gc = TRUE;
}

static void
coil_parser_prepare_for_buffer(CoilParser *const parser,
                               char *buffer, gsize len)
{
    g_return_if_fail(parser != NULL);
    g_return_if_fail(buffer != NULL);
    g_return_if_fail(len > 0);
    g_return_if_fail(parser->buffer_state == NULL);

    if (buffer[len - 2] || buffer[len - 1]) {
        g_error("The last 2 bytes of buffer must be ASCII NUL.");
    }
    parser->buffer_state = (gpointer)yy_scan_buffer(buffer,
        (yy_size_t)len, (yyscan_t)parser->scanner);

    if (parser->buffer_state == NULL) {
        g_error("Error preparing buffer for scanner.");
    }
    parser->do_buffer_gc = TRUE;
}

COIL_API(CoilObject *)
coil_parse_string_len(const gchar *string, gsize len)
{
    g_return_val_if_fail(string, NULL);

    CoilParser parser;
    yyscan_t scanner;

    if (len == 0) {
        return coil_struct_new(NULL, NULL);
    }
    coil_parser_init(&parser, &scanner);
    coil_parser_prepare_for_string(&parser, string, len);
    yyparse(&parser);
    return coil_parser_finish(&parser);
}

COIL_API(CoilObject *)
coil_parse_string(const gchar *string)
{
    return coil_parse_string_len(string, strlen(string));
}

COIL_API(CoilObject *)
coil_parse_buffer(gchar *buffer, gsize len)
{
    g_return_val_if_fail(buffer != NULL, NULL);

    CoilParser parser;
    yyscan_t scanner;

    if (len == 0) {
        return coil_struct_new(NULL, NULL);
    }
    coil_parser_init(&parser, &scanner);
    coil_parser_prepare_for_buffer(&parser, buffer, len);
    yyparse(&parser);
    return coil_parser_finish(&parser);
}

COIL_API(CoilObject *)
coil_parse_file(const gchar *filepath)
{
    g_return_val_if_fail(filepath, NULL);

    CoilParser parser;
    yyscan_t scanner;
    FILE *fp;

    if (!g_file_test(filepath, (G_FILE_TEST_IS_REGULAR | G_FILE_TEST_EXISTS))) {
        coil_set_error(COIL_ERROR_PARSE, NULL,
            "Unable to find file '%s'.", filepath);
        return NULL;
    }
    if (!(fp = fopen(filepath, "r"))) {
        coil_set_error(COIL_ERROR_PARSE, NULL,
                    "Unable to open file '%s'.", filepath);
        return NULL;
    }
    coil_parser_init(&parser, &scanner);
    coil_parser_prepare_for_stream(&parser, fp, filepath);
    yyparse(&parser);
    fclose(fp);
    return coil_parser_finish(&parser);
}

COIL_API(CoilObject *)
coil_parse_stream(FILE *fp, const gchar *stream_name)
{
    g_return_val_if_fail(fp, NULL);

    CoilParser parser;
    yyscan_t scanner;

    if (stream_name == NULL && fp == stdin) {
        stream_name = "(stdin)";
    }

    coil_parser_init(&parser, &scanner);
    coil_parser_prepare_for_stream(&parser, fp, stream_name);
    yyparse(&parser);
    return coil_parser_finish(&parser);
}

void
yyerror(YYLTYPE *yylocp, CoilParser *parser, const gchar *msg)
{
    g_return_if_fail(parser != NULL);
    g_return_if_fail(yylocp != NULL);

    if (coil_error_occurred()) {
        handle_internal_error(parser);
    }
    else {
        set_parse_error(parser, yylocp, "%s", msg);
    }
}

