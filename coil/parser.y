%{
/*
 * Copyright (C) 2009, 2010
 *
 * Author: John O'Connor
 */

#include <stdio.h>
#include <string.h>

#include "common.h"

#include "path.h"
#include "list.h"
#include "struct.h"
#include "include.h"
#include "link.h"
#include "expression.h"

#include "parser_defs.h"
#include "parser.h"
#include "scanner.h"

#define PEEK_CONTAINER(parser) \
  (CoilStruct *)g_queue_peek_head(&(parser)->containers)

#define PEEK_NTH_CONTAINER(parser, n) \
  (CoilStruct *)g_queue_peek_nth(&(parser)->containers, n)

#define POP_CONTAINER(parser) \
  (CoilStruct *)g_queue_pop_head(&(parser)->containers)

#define PUSH_CONTAINER(parser, c) \
  g_queue_push_head(&(parser)->containers, (c))

#define parser_error(parser, format, args...) \
  g_set_error(&parser->error, \
              COIL_ERROR, \
              COIL_ERROR_PARSE, \
              format, \
              ##args)

void yyerror(YYLTYPE     *yylocp,
             CoilParser  *parser,
             const gchar *msg);

static void
parser_handle_error(CoilParser *parser)
{
  g_return_if_fail(parser);

  if (parser->error)
  {
    parser->errors = g_list_prepend(parser->errors, parser->error);
    parser->error = NULL;
  }
}

static void
collect_parse_errors(GError     **error,
                     CoilParser  *const parser,
                     gboolean     number_errors)
{
  g_return_if_fail(parser);
  g_return_if_fail(error == NULL || *error == NULL);

  guint    n;
  GList   *list;
  GString *msg;

  if (error == NULL)
    return; /* ignore errors */

  if (parser->errors == NULL)
    return; /* no errors to collect */

  msg = g_string_new_len(COIL_STATIC_STRLEN("Parse Error(s): \n"));

  for (list = g_list_last(parser->errors), n = 1;
       list; list = g_list_previous(list))
  {
    g_assert(list->data);
    const GError *err = (GError *)list->data;

    if (number_errors)
      g_string_append_printf(msg, "%d) %s\n", n++, (gchar *)err->message);
    else
      g_string_append(msg, err->message);

    g_string_append_c(msg, '\n');
  }

  g_propagate_error(error,
    g_error_new_literal(COIL_ERROR,
                        COIL_ERROR_PARSE,
                        msg->str));

  g_string_free(msg, TRUE);
}

static gboolean
handle_undefined_prototypes(CoilParser *parser)
{
  g_return_val_if_fail(parser != NULL, FALSE);

  GHashTable *proto_table = parser->prototypes;

  /* Attempt necessary expansions to finalize prototypes which are
    defined after an expansion */
  if (g_hash_table_size(proto_table) > 0)
  {
    GList *prototypes;

    for (prototypes = g_hash_table_get_values(proto_table);
         prototypes;
         prototypes = g_list_delete_link(prototypes, prototypes))
    {
      CoilStruct *prototype, *container;

      prototype = (CoilStruct *)prototypes->data;
      container = coil_struct_get_container(prototype);

      if (!coil_struct_is_prototype(container))
        coil_struct_expand(container, &parser->error);

      if (G_UNLIKELY(parser->error))
      {
        parser_handle_error(parser);
        g_list_free(prototypes);
        return FALSE;
      }

      if (coil_struct_is_prototype(prototype))
      {
        g_list_free(prototypes);
        break;
      }

      prototypes = g_list_delete_link(prototypes, prototypes);
    }

    /* If we still have undefined prototypes this is an error */
    if (g_hash_table_size(proto_table) > 0)
    {
      GList   *list, *lp;
      GString *msg = g_string_sized_new(512);

      msg = g_string_append(msg,
                            "Referencing undefined structs, prototypes are: ");

      list = g_hash_table_get_values(proto_table);

      /* convert list of prototypes to list of paths */
      for (lp = list; lp; lp = g_list_next(lp))
      {
        CoilStruct     *prototype = COIL_STRUCT(lp->data);
        const CoilPath *path = coil_struct_get_path(prototype);

        lp->data = (gpointer)path;

        /* cast the prototype to an empty struct
           to allow continue despite errors */
        CoilStructFunc apply_fn = (CoilStructFunc)make_prototype_final;
        coil_struct_foreach_ancestor(prototype, TRUE, apply_fn, NULL);
      }

      /* sort by path and build error message */
      for (list = g_list_sort(list, (GCompareFunc)coil_path_compare);
           list; list = g_list_delete_link(list, list))
      {
        const CoilPath *path = (CoilPath *)list->data;
        g_string_append_printf(msg, "%s, ", path->path);
      }

      parser_error(parser, "%s", msg->str);
      parser_handle_error(parser);

      g_string_free(msg, TRUE);

      return FALSE;
    }
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
  CoilStruct     *new_container, *container = PEEK_CONTAINER(parser);
  const CoilPath *path = parser->path;

  new_container = coil_struct_create_containers(container,
                                                path->path,
                                                path->path_len,
                                                FALSE, /* is prototype */
                                                FALSE, /* has failed lookup */
                                                &parser->error);

  if (G_UNLIKELY(parser->error))
  {
    if (new_container)
      g_object_unref(new_container);

    return FALSE;
  }

  g_object_set(new_container,
               "accumulate", TRUE,
               NULL);

  g_object_ref(new_container);
  PUSH_CONTAINER(parser, new_container);

  return TRUE;
}

static void
parser_pop_container(CoilParser *parser)
{
  CoilStruct *container = POP_CONTAINER(parser);

  g_object_set(container,
               "accumulate", FALSE,
               NULL);

  g_object_unref(container);
}

/* arguments can take the following form..

    case 1. @include: <single value>   (parsed as the filename)
    - or -
    case 2. @include: [filename [paths...]]
    - or -
    case 3. @include: [[filename [paths...]]...]
*/

static gboolean
parser_handle_include(CoilParser   *parser,
                      CoilLocation *location,
                      GList        *include_args)
{
  g_return_val_if_fail(parser, FALSE);
  g_return_val_if_fail(parser->error == NULL, FALSE);

  if (G_UNLIKELY(include_args == NULL))
  {
    parser_error(parser, "No filename specified for include/import.");
    return FALSE;
  }

  CoilStruct *container = PEEK_CONTAINER(parser);
  GValue     *filepath_value = (GValue *)include_args->data;
  GList      *import_list = g_list_delete_link(include_args, include_args);

  /* TODO(jcon): consider passing entire argument list
                 instead of breaking it up above */

  CoilInclude *include = coil_include_new("filepath_value", filepath_value,
                                          "import_list", import_list,
                                          "container", container,
                                          "location", location,
                                          NULL);

  coil_struct_add_dependency(container, include, &parser->error);

  g_object_unref(include);

  return G_UNLIKELY(parser->error) ? FALSE : TRUE;
}

static CoilPath *
parser_make_path_from_string(CoilParser  *parser,
                             GString     *gstring)
{
  g_return_val_if_fail(parser, NULL);
  g_return_val_if_fail(gstring, NULL);

  if (!coil_validate_path_strn(gstring->str, gstring->len))
  {
    parser_error(parser,
                 "Expecting valid path in string but found '%s' instead.",
                 gstring->str);

    g_string_free(gstring, TRUE);
    return NULL;
  }

  CoilPath *path = coil_path_take_strings(gstring->str, gstring->len,
                                          NULL, 0, 0);

  g_string_free(gstring, FALSE);
  return path;
}

static gboolean
parser_has_errors(CoilParser *const parser)
{
  g_return_val_if_fail(parser, TRUE);
  return parser->errors != NULL;
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
  CoilPath  *path;
  GValue    *value;
  glong      longint;
  gdouble    doubleval;
  GList     *path_list;
  GList     *value_list;
  GString   *gstring;
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

%type <value_list> value_list
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
%destructor { free_value($$); } <value>
%destructor { free_value_list($$); } <value_list>

%start coil

%%

coil
  : context
;

context
  : /* empty */
  | context statement
;

statement
  : builtin_property
  | deletion
  | assignment
  | error { parser_handle_error(YYCTX); }
;

deletion
  : '~' RELATIVE_PATH
  {
    CoilStruct *container = PEEK_CONTAINER(YYCTX);

    if (!coil_struct_mark_deleted_path(container, $2, FALSE, &YYCTX->error))
      YYERROR;
  }
;

assignment
  : assignment_path ':' assignment_value
;

assignment_path
  : RELATIVE_PATH
  {
    /* do some resolving up front for those that need it
      -- this prevents resolving paths twice and such */
    const CoilStruct *container = PEEK_CONTAINER(YYCTX);
    const CoilPath   *container_path = coil_struct_get_path(container);

    if (YYCTX->path)
      coil_path_unref(YYCTX->path);

    YYCTX->path = coil_path_resolve($1, container_path, &YYCTX->error);
    coil_path_unref($1);

    if (G_UNLIKELY(YYCTX->error))
      YYERROR;
  }
;

assignment_value
  : container
  | value
  {
    CoilStruct *container = PEEK_CONTAINER(YYCTX);

    if (!coil_struct_insert_path(container,
                                 YYCTX->path, $1, FALSE,
                                 &YYCTX->error))
      YYERROR;

    YYCTX->path = NULL;
  }
;

container
  : container_declaration { parser_pop_container(YYCTX); }
  |  error
  {
    parser_pop_container(YYCTX);
    parser_handle_error(YYCTX);
  }
;

container_declaration
  : container_context_inherit
  | container_context
;

container_context_inherit
  : path_list_comma '{'
  {
    CoilStruct *container, *context;

    context = PEEK_CONTAINER(YYCTX);

    if (!parser_push_container(YYCTX))
      YYERROR;

    container = PEEK_CONTAINER(YYCTX);

    if (!coil_struct_extend_paths(container, $1, context, &YYCTX->error))
      YYERROR;

    coil_path_list_free($1);
  }
  context '}'
;

container_context
  : '{'
  {
    if (!parser_push_container(YYCTX))
      YYERROR;
  }
  context '}'
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
  : extend_property_declaration path_list
  {
    CoilStruct *container = PEEK_CONTAINER(YYCTX);

    if (!coil_struct_extend_paths(container, $2, NULL, &YYCTX->error))
      YYERROR;

    coil_path_list_free($2);
  }
;

include_declaration
  : INCLUDE_SYM
  | INCLUDE_SYM ':' /* compat */
;

include_property
  : include_declaration include_args
  {
    if (!parser_handle_include(YYCTX, &@$, $2))
      YYERROR;
  }
;

include_args
  : include_file
  { $$ = $1; }
  | include_arglist
  { $$ = $1; }
;

include_file
  : link
  { $$ = g_list_prepend(NULL, $1); }
  | string
  { $$ = g_list_prepend(NULL, $1); }
;

include_arglist
  : '[' include_file include_import_list ']'
  { $$ = g_list_concat($2, $3); }
;

include_import_list
  : /* empty */                       { $$ = NULL; }
  | include_import_list_spaced        { $$ = $1; }
  | ',' include_import_list_comma     { $$ = $2; }
  | ',' include_import_list_comma ',' { $$ = $2; }
;

include_import_list_spaced
  : include_import_path
  { $$ = g_list_prepend(NULL, $1); }
  | include_import_list_spaced include_import_path
  { $$ = g_list_prepend($1, $2); }
;

include_import_list_comma
  : include_import_path
  { $$ = g_list_prepend(NULL, $1); }
  | include_import_list_comma ',' include_import_path
  { $$ = g_list_prepend($1, $3); }
;

include_import_path
  : link             { $$ = $1; }
  | pathstring_value { $$ = $1; }
;

link
  : link_path                   { $$ = $1; }
  | '=' link_path               { $$ = $2; }
;

link_path
  : path
  {
    CoilStruct *container = PEEK_CONTAINER(YYCTX);

  /* XXX: YYCTX->path is null when link is not assigned to a path
      ie. @extends: [ =..some_node ]
   */

    new_value($$, COIL_TYPE_LINK, take_object,
      coil_link_new("target_path", $1,
                    "path", YYCTX->path,
                    "container", container,
                    "location", &@$,
                    NULL));

    coil_path_unref($1);
  }
;

pathstring_value
  : pathstring { new_value($$, COIL_TYPE_PATH, take_boxed, $1); }
;

pathstring
  : STRING_LITERAL
  {
    if (!($$ = parser_make_path_from_string(YYCTX, $1)))
      YYERROR;
  }
;

path_list
  : '[' path_list_spaced ']' { $$ = $2; }

/* TODO(jcon): examine shift reduce conflict */
  | path_list_comma         { $$ = $1; }
;

path_list_spaced
  : path                     { $$ = g_list_prepend(NULL, $1); }
  | path_list_spaced path     { $$ = g_list_prepend($1, $2); }
;

path_list_comma
  : path_list_comma_items     { $$ = $1; }
  | path_list_comma_items ',' { $$ = $1; }
;

path_list_comma_items
  : path                           { $$ = g_list_prepend(NULL, $1); }
  | path_list_comma_items ',' path { $$ = g_list_prepend($1, $3); }
;

value_list
  : '[' ']'                      { $$ = NULL; }
  | '[' value_list_contents ']'  { $$ = g_list_reverse($2); }
;

value_list_contents
  : value_list_items_comma       { $$ = $1; }
  | value_list_items_comma value { $$ = g_list_prepend($1, $2); }
  | value_list_items             { $$ = $1; }
;

value_list_items
  : value                  { $$ = g_list_prepend(NULL, $1); }
  | value_list_items value { $$ = g_list_prepend($1, $2); }
;

value_list_items_comma
  : value ','                        { $$ = g_list_prepend(NULL, $1); }
  | value_list_items_comma value ',' { $$ = g_list_prepend($1, $2); }
;

value
  : primative  { $$ = $1; }
  | string     { $$ = $1; }
  | link       { $$ = $1; }
  | value_list { new_value($$, COIL_TYPE_LIST, take_boxed, $1); }
;

path
  : ABSOLUTE_PATH  { $$ = $1; }
  | REFERENCE_PATH { $$ = $1; }
  | RELATIVE_PATH  { $$ = $1; }
  | ROOT_PATH      { $$ = $1; }
;

string
  : STRING_LITERAL
  { new_value($$, G_TYPE_GSTRING, take_boxed, $1); }
  | STRING_EXPRESSION
  { new_value($$, COIL_TYPE_EXPR, take_object, coil_expr_new($1)); }
;

primative
  : NONE_SYM  { new_value($$, COIL_TYPE_NONE, set_object, coil_none_object); }
  | TRUE_SYM  { new_value($$, G_TYPE_BOOLEAN, set_boolean, TRUE); }
  | FALSE_SYM { new_value($$, G_TYPE_BOOLEAN, set_boolean, FALSE); }
  | INTEGER   { new_value($$, G_TYPE_LONG, set_long, $1); }
  | DOUBLE    { new_value($$, G_TYPE_DOUBLE, set_double, $1); }
;

%%
extern int yydebug;

typedef struct _ParserNotify
{
  gulong      handler_id;
  CoilParser *parser;
} ParserNotify;

void parser_notify_free(gpointer  data,
                        GClosure *closure)
{
  g_return_if_fail(data);
  g_free(data);
}

static void
untrack_prototype(GObject    *instance,
                  GParamSpec *unused, /* notify arg ignored */
                  gpointer    data)
{
  g_return_if_fail(COIL_IS_STRUCT(instance));
  g_return_if_fail(data);

  ParserNotify *notify = (ParserNotify *)data;
  CoilParser   *parser = (CoilParser *)notify->parser;
  CoilStruct   *root, *prototype = COIL_STRUCT(instance);

  g_return_if_fail(parser);
  g_return_if_fail(prototype);

  root = coil_struct_get_root(prototype);

  if (root == parser->root
    && !coil_struct_is_prototype(prototype))
  {
    g_hash_table_remove(parser->prototypes, prototype);
    g_signal_handler_disconnect(instance, notify->handler_id);
  }
}

/*
 * Called when prototype struct is constructed.
 */
static gboolean
track_prototype(GSignalInvocationHint *ihint,
                guint                  n_param_values,
                const GValue          *param_values,
                gpointer               data)
{
  g_return_val_if_fail(data, FALSE);
  g_return_val_if_fail(n_param_values > 0, FALSE);
  g_return_val_if_fail(G_VALUE_HOLDS(&param_values[0], COIL_TYPE_STRUCT),
                       FALSE);

  CoilStruct   *prototype, *root;
  CoilParser   *parser = (CoilParser *)data;

  prototype = COIL_STRUCT(g_value_get_object(&param_values[0]));
  root = coil_struct_get_root(prototype);

  if (root == parser->root)
  {
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
coil_parser_init(CoilParser *const parser,
                 gpointer    scanner)
{
  g_return_if_fail(parser != NULL);
  g_return_if_fail(scanner != NULL);

  memset(parser, 0, sizeof(*parser));

  if (yylex_init_extra(parser, &scanner))
  {
    g_error("Could not set parser context for scanner. %s",
      g_strerror(errno));
    return;
  }

#ifdef COIL_DEBUG
//  yydebug = 0;
#endif

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

static CoilStruct *
coil_parser_finish(CoilParser *parser,
                   GError    **error)
{
  g_return_val_if_fail(parser, NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  CoilStruct *container;

  parser_post_processing(parser);

  g_signal_remove_emission_hook(g_signal_lookup("create", COIL_TYPE_STRUCT),
                                parser->prototype_hook_id);

  g_hash_table_destroy(parser->prototypes);

  while ((container = g_queue_pop_head(&parser->containers)))
    g_object_unref(container);

  if (parser->path)
    coil_path_unref(parser->path);

  if (parser->do_buffer_gc)
    yy_delete_buffer((YY_BUFFER_STATE)parser->buffer_state,
                      (yyscan_t)parser->scanner);

  if (parser->scanner)
    yylex_destroy((yyscan_t)parser->scanner);

  if (parser_has_errors(parser))
  {
    collect_parse_errors(error, parser, TRUE);

    while (parser->errors)
    {
      g_error_free((GError *)parser->errors->data);
      parser->errors = g_list_delete_link(parser->errors, parser->errors);
    }
  }

  return parser->root;
}

static void
coil_parser_prepare_for_stream(CoilParser *const parser,
                               FILE           *stream,
                               const gchar    *name)
{
  g_return_if_fail(parser != NULL);
  g_return_if_fail(stream != NULL);
  g_return_if_fail(name != NULL);

  yyset_in(stream, (yyscan_t)parser->scanner);
  parser->filepath = name;
}

static void
coil_parser_prepare_for_string(CoilParser *const parser,
                               const char      *string,
                               gsize            len)
{
  g_return_if_fail(parser != NULL);
  g_return_if_fail(string != NULL);
  g_return_if_fail(parser->buffer_state == NULL);

  if (len > 0)
  {
    parser->buffer_state = (gpointer)yy_scan_bytes(string,
                          (yy_size_t)len,
                          (yyscan_t)parser->scanner);
  }
  else
  {
    parser->buffer_state = (gpointer)yy_scan_string(string,
                            (yyscan_t)parser->scanner);
  }

  if (parser->buffer_state == NULL)
    g_error("Error preparing buffer for scanner.");

  parser->do_buffer_gc = TRUE;
}

static void
coil_parser_prepare_for_buffer(CoilParser *const parser,
                               char           *buffer,
                               gsize           len)
{
  g_return_if_fail(parser != NULL);
  g_return_if_fail(buffer != NULL);
  g_return_if_fail(len > 0);
  g_return_if_fail(parser->buffer_state == NULL);

  if (buffer[len - 2]
    || buffer[len - 1])
    g_error("The last 2 bytes of buffer must be ASCII NUL.");

  parser->buffer_state = (gpointer)yy_scan_buffer(buffer,
                        (yy_size_t)len, (yyscan_t)parser->scanner);

  if (parser->buffer_state == NULL)
    g_error("Error preparing buffer for scanner.");

  parser->do_buffer_gc = TRUE;
}

COIL_API(CoilStruct *)
coil_parse_string_len(const gchar *string,
                      gsize        len,
                      GError     **error)
{
  g_return_val_if_fail(string, NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  CoilParser parser;
  yyscan_t   scanner;

  if (len == 0)
    return coil_struct_new(NULL, NULL);

  coil_parser_init(&parser, &scanner);
  coil_parser_prepare_for_string(&parser, string, len);

  yyparse(&parser);

  return coil_parser_finish(&parser, error);
}

COIL_API(CoilStruct *)
coil_parse_string(const gchar *string,
                  GError     **error)
{
  return coil_parse_string_len(string, 0, error);
}

COIL_API(CoilStruct *)
coil_parse_buffer(gchar   *buffer,
                  gsize    len,
                  GError **error)
{
  g_return_val_if_fail(buffer != NULL, NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  CoilParser parser;
  yyscan_t   scanner;

  if (len == 0)
    return coil_struct_new(NULL, NULL);

  coil_parser_init(&parser, &scanner);
  coil_parser_prepare_for_buffer(&parser, buffer, len);

  yyparse(&parser);

  return coil_parser_finish(&parser, error);
}

COIL_API(CoilStruct *)
coil_parse_file(const gchar *filepath,
                GError     **error)
{
  g_return_val_if_fail(filepath, NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  CoilParser parser;
  yyscan_t   scanner;
  FILE      *fp;

  if (!g_file_test(filepath,
      (G_FILE_TEST_IS_REGULAR | G_FILE_TEST_EXISTS)))
  {
    g_set_error(error, COIL_ERROR, COIL_ERROR_PARSE,
                "Unable to find file '%s'.", filepath);

    return NULL;
  }

  if (!(fp = fopen(filepath, "r")))
  {
    g_set_error(error, COIL_ERROR, COIL_ERROR_PARSE,
                "Unable to open file '%s'.", filepath);
    return NULL;
  }

  coil_parser_init(&parser, &scanner);
  coil_parser_prepare_for_stream(&parser, fp, filepath);

  yyparse(&parser);
  fclose(fp);

  return coil_parser_finish(&parser, error);
}

COIL_API(CoilStruct *)
coil_parse_stream(FILE        *fp,
                  const gchar *stream_name,
                  GError     **error)
{
  g_return_val_if_fail(fp, NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  CoilParser parser;
  yyscan_t   scanner;

  if (stream_name == NULL && fp == stdin)
    stream_name = "(stdin)";

  coil_parser_init(&parser, &scanner);
  coil_parser_prepare_for_stream(&parser, fp, stream_name);

  yyparse(&parser);

  return coil_parser_finish(&parser, error);
}

void yyerror(YYLTYPE        *yylocp,
             CoilParser     *parser,
             const gchar    *msg)
{
  g_return_if_fail(parser != NULL);
  g_return_if_fail(yylocp != NULL);

  if (parser->error)
    return;

  parser->errors = g_list_prepend(parser->errors,
    coil_error_new(COIL_ERROR_PARSE,
                   (*yylocp),
                   "%s",
                    msg));
/*                    yyget_text(parser->scanner)));*/
}

