/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#include "coil.h"

#include <stdlib.h>
#include <string.h>

static gchar **attributes = NULL;
static gchar **blocks = NULL;
static gchar **files = NULL;
static gchar **paths = NULL;

static gint block_indent = 4;
static gint brace_indent = 0;
static gint indent_level = 0;
static gint multiline_length = 80;

static gboolean blank_line_after_brace = TRUE;
static gboolean blank_line_after_item = FALSE;
static gboolean blank_line_after_struct = TRUE;
static gboolean brace_on_blank_line = FALSE;
static gboolean commas_in_list = FALSE;
static gboolean compact = FALSE;
static gboolean compat = FALSE;
static gboolean expand_all = FALSE;
static gboolean flatten = FALSE;
static gboolean list_on_blank_line = FALSE;
static gboolean merge_files = FALSE;
static gboolean no_string_quotes = FALSE;
static gboolean no_clobber_attributes = FALSE;
static gboolean permissive = FALSE;
static gboolean show_dependencies = FALSE;
static gboolean show_version = FALSE;

static const GOptionEntry main_entries[] =
{
  {"attribute", 'a', 0, G_OPTION_ARG_STRING_ARRAY, &attributes,
      "A key:value pair to add to the coil output", "<key:value>"},

  {"block", 'b', 0, G_OPTION_ARG_STRING_ARRAY, &blocks,
      "Print struct (and all values contained within) at <path>", "<path>"},

  {"compat", 0, 0, G_OPTION_ARG_NONE, &compat,
      "Maintain compatability with previous coil versions.", NULL},

  {"expand-all", 0, 0, G_OPTION_ARG_NONE, &expand_all,
      "Expand all values.", NULL},

  {"flatten", 'f', 0, G_OPTION_ARG_NONE, &flatten,
      "Show key-values on separate, fully-qualified lines", NULL},

  {"merge-files", 'm', 0, G_OPTION_ARG_NONE, &merge_files,
      "Merge all files specified into one struct.", NULL},

  {"no-clobber", 'n', 0, G_OPTION_ARG_NONE, &no_clobber_attributes,
      "Don't overwrite existing attributes with those specified on the " \
      "command line.", NULL},

  {"path", 'p', 0, G_OPTION_ARG_STRING_ARRAY, &paths,
      "Print coil value at <path>", "<path>"},

  {"permissive", 0, 0, G_OPTION_ARG_NONE, &permissive,
      "Ignore minor errors during parsing.", NULL},

  {"version", 0, 0, G_OPTION_ARG_NONE, &show_version,
      "Print version and exit.", NULL},

  {G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &files, NULL, NULL},

  {NULL}
};

static const GOptionEntry formatting_entries[] =
{
  {"indent-level", 0, 0, G_OPTION_ARG_INT, &indent_level,
      "Initial indent level in number of spaces. Default: 0", "<integer>"},

  {"block-indent", 0, 0, G_OPTION_ARG_INT, &block_indent,
      "Number of spaces used as indentation. Usually 4 spaces.", "<integer>"},

  {"brace-indent", 0, 0, G_OPTION_ARG_INT, &brace_indent,
      "Number of spaces used to indent braces '{'. 0 by default.", "<integer>"},

  {"multiline-length", 0, 0, G_OPTION_ARG_INT, &multiline_length,
      "Strings greater than <length> are considered "\
      "multiline strings.", "<length>"},

  {"blank-line-after-brace", 0, 0, G_OPTION_ARG_NONE, &blank_line_after_brace,
      "Print a blank line after brace '{' characters", NULL},

  {"no-blank-line-after-brace", 0, G_OPTION_FLAG_REVERSE,
    G_OPTION_ARG_NONE, &blank_line_after_brace,
      "Opposite of --blank-line-after-brace", NULL},

  {"blank-line-after-struct", 0, 0, G_OPTION_ARG_NONE, &blank_line_after_struct,
      "Print a blank line after structs ie '}' characters.", NULL},

  {"no-blank-line-after-struct", 0, G_OPTION_FLAG_REVERSE,
    G_OPTION_ARG_NONE, &blank_line_after_struct,
      "Opposite of --blank-line-after-struct", NULL},

  {"blank-line-after-item", 0, 0, G_OPTION_ARG_NONE, &blank_line_after_item,
      "Print blank line after commas in lists.", NULL},

  {"no-blank-line-after-item", 0, G_OPTION_FLAG_REVERSE,
    G_OPTION_ARG_NONE, &blank_line_after_item,
      "Opposite of --blank-line-after-item", NULL},

  {"brace-on-blank-line", 0, 0, G_OPTION_ARG_NONE, &brace_on_blank_line,
      "Put brace on same line as path for struct declarations.", NULL},

  {"brace-on-same-line", 0, G_OPTION_FLAG_REVERSE,
    G_OPTION_ARG_NONE, &brace_on_blank_line,
      "Put brace '{' on same line as path", NULL},

  {"list-on-blank-line", 0, 0, G_OPTION_ARG_NONE, &list_on_blank_line,
      "Put list brackets on a new line", NULL},

  {"list-on-same-line", 0, G_OPTION_FLAG_REVERSE,
    G_OPTION_ARG_NONE, &list_on_blank_line,
      "Put list brackets on same line as path.", NULL},

  {"commas-in-list", 0, 0, G_OPTION_ARG_NONE, &commas_in_list,
      "Use commas in lists instead of spaces", NULL},

  {"no-commas-in-list", 0, G_OPTION_FLAG_REVERSE,
    G_OPTION_ARG_NONE, &commas_in_list,
      "Opposite of --commas-in-list", NULL},

  {"no-string-quotes", 0, 0, G_OPTION_ARG_NONE, &no_string_quotes,
      "Don't print string quotes", NULL},

  {"compact", 0, 0, G_OPTION_ARG_NONE, &compact,
      "Make output as compact as possible.", NULL},

  { NULL }
};

static const GOptionEntry info_entries[] =
{
  {"show-dependency-tree", 0, 0, G_OPTION_ARG_NONE, &show_dependencies,
      "Show all files required by specified input coil files", NULL},

  { NULL }
};

static gboolean
check_main_entries(GOptionContext *context,
                   GOptionGroup   *group,
                   gpointer        data,
                   GError        **error)
{
  guint num_files;

  if (show_version)
      return TRUE;

  if (files == NULL)
  {
    g_set_error_literal(error,
                        G_OPTION_ERROR,
                        G_OPTION_ERROR_BAD_VALUE,
                        "No input files specified");

    return FALSE;
  }

  num_files = g_strv_length(files);

  if (merge_files && num_files < 2)
  {
    g_set_error_literal(error,
                        G_OPTION_ERROR,
                        G_OPTION_ERROR_BAD_VALUE,
                        "--merge-files should be specified with more "\
                        "than one input file");

    return FALSE;
  }

  if (no_clobber_attributes && attributes == NULL)
  {
    g_set_error_literal(error,
                        G_OPTION_ERROR,
                        G_OPTION_ERROR_BAD_VALUE,
                        "--no-clobber should be specified with at " \
                        "least one attribute");

    return FALSE;
  }


  return TRUE;
}

static gboolean
check_formatting_entries(GOptionContext *context,
                         GOptionGroup   *group,
                         gpointer        data,
                         GError        **error)
{

  if (compat && commas_in_list)
  {
    g_set_error_literal(error,
                        G_OPTION_ERROR,
                        G_OPTION_ERROR_BAD_VALUE,
                        "--compat and --commas-in-list are " \
                        "incompatible options");

    return FALSE;
  }

  if (indent_level < 0)
  {
    g_set_error(error,
                G_OPTION_ERROR,
                G_OPTION_ERROR_BAD_VALUE,
                "--indent-level=%d must be a positive number.",
                indent_level);

    return FALSE;
  }

  if (multiline_length < 0)
  {
    g_set_error(error,
                G_OPTION_ERROR,
                G_OPTION_ERROR_BAD_VALUE,
                "--multiline-length=%d must be a positive number.",
                multiline_length);

    return FALSE;
  }

  if (block_indent < 0)
  {
    g_set_error(error,
                G_OPTION_ERROR,
                G_OPTION_ERROR_BAD_VALUE,
                "--block-indent=%d must be a positive number.",
                block_indent);

    return FALSE;
  }

  if (brace_indent < 0)
  {
    g_set_error(error,
                G_OPTION_ERROR,
                G_OPTION_ERROR_BAD_VALUE,
                "--brace-indent=%d must be a positive number.",
                brace_indent);

    return FALSE;
  }

  return TRUE;
}

static void
parse_options(int    *argc,
              char ***argv)
{
  GError         *error = NULL;
  GOptionContext *option_context;
  GOptionGroup   *main_group, *formatting_group, *info_group;

  option_context = g_option_context_new("file1.coil [file2.coil] ...");

  formatting_group = g_option_group_new("formatting",
                                        "Formatting Options:",
                                        "Show formatting options",
                                        NULL, NULL);

  g_option_group_add_entries(formatting_group, formatting_entries);
  g_option_group_set_parse_hooks(formatting_group, NULL,
                                 check_formatting_entries);
  g_option_context_add_group(option_context, formatting_group);

  info_group = g_option_group_new("info",
                                  "Information Options:",
                                  "Show information options",
                                  NULL, NULL);

  g_option_group_add_entries(info_group, info_entries);
  g_option_context_add_group(option_context, info_group);

  g_option_context_add_main_entries(option_context, main_entries, NULL);

  main_group = g_option_context_get_main_group(option_context);
  g_option_group_set_parse_hooks(main_group, NULL, check_main_entries);

  if (!g_option_context_parse(option_context, argc, argv, &error))
  {
    g_printerr("Failed parsing options: %s\n", error->message);
    g_error_free(error);
    exit(EXIT_FAILURE);
  }

  g_option_context_free(option_context);
}

static CoilStruct *
parse_attributes(GError **error)
{
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  gchar      *buf;
  CoilStruct *result = NULL;

  if (attributes)
  {
    buf = g_strjoinv(" ", attributes);
    result = coil_parse_string(buf, error);
    g_free(buf);
  }

  return result;
}

static void
print_blocks(CoilStruct       *root,
             GString          *buffer,
             CoilStringFormat *format,
             GError          **error)
{
  g_return_if_fail(COIL_IS_STRUCT(root));
  g_return_if_fail(buffer);
  g_return_if_fail(format);
  g_return_if_fail(error == NULL || *error == NULL);

  GError *internal_error = NULL;
  gint    i = 0;

  if (blocks)
    for (i = 0; blocks[i]; i++)
    {
      const GValue *value;
      CoilStruct   *block;
      const gchar  *path;
      guint         len;

      g_strstrip(blocks[i]);

      path = blocks[i];
      len = strlen(path);
      value = coil_struct_lookup(root, path, len, TRUE, &internal_error);

      if (G_UNLIKELY(internal_error))
      {
        g_propagate_error(error, internal_error);
        return;
      }

      if (value)
      {
        if (!G_VALUE_HOLDS(value, COIL_TYPE_STRUCT))
        {
          g_set_error(error,
                      COIL_ERROR,
                      COIL_ERROR_VALUE,
                      "Values for '--block' must be type struct."\
                      " Path '%s' is type '%s'",
                      path, G_VALUE_TYPE_NAME(value));

          return;
        }

        block = COIL_STRUCT(g_value_dup_object(value));
        coil_struct_build_string(block, buffer, format, &internal_error);

        if (G_UNLIKELY(internal_error))
        {
          g_object_unref(block);
          g_propagate_error(error, internal_error);
          return;
        }

        g_object_unref(block);
      }
    }
}

static void
print_paths(CoilStruct       *root,
            GString          *buffer,
            CoilStringFormat *format,
            GError          **error)
{
  g_return_if_fail(COIL_IS_STRUCT(root));
  g_return_if_fail(buffer);
  g_return_if_fail(format);
  g_return_if_fail(error == NULL || *error == NULL);

  GError *internal_error = NULL;
  gint    i = 0;

  if (paths)
    for (i = 0; paths[i]; i++)
    {
      const GValue *value;
      const gchar  *path;
      guint         len;

      g_strstrip(paths[i]);

      path = paths[i];
      len = strlen(path);
      value = coil_struct_lookup(root, path, len, TRUE, &internal_error);

      if (G_UNLIKELY(internal_error))
        goto error;

      if (value)
      {
        coil_value_build_string(value, buffer, format, &internal_error);
        g_string_append_c(buffer, '\n');

        if (G_UNLIKELY(internal_error))
          goto error;
      }
    }

  if (buffer->len > 0
   && buffer->str[buffer->len - 1] == '\n')
    g_string_truncate(buffer, buffer->len - 1);

  return;

error:
  g_propagate_error(error, internal_error);
}

static void
print_struct(CoilStruct       *node,
             GString          *buffer,
             CoilStringFormat *format,
             GError          **error)
{
  g_return_if_fail(COIL_IS_STRUCT(node));
  g_return_if_fail(buffer);
  g_return_if_fail(format);
  g_return_if_fail(error == NULL || *error == NULL);

  GError *internal_error = NULL;

  print_blocks(node, buffer, format, &internal_error);

  if (G_UNLIKELY(internal_error))
    goto error;

  print_paths(node, buffer, format, &internal_error);

  if (G_UNLIKELY(internal_error))
    goto error;

  if (!blocks && !paths)
  {
    coil_struct_build_string(node, buffer, format, &internal_error);

    if (G_UNLIKELY(internal_error))
      goto error;
  }

  g_print("%s\n", buffer->str);
  return;

error:
  g_propagate_error(error, internal_error);
}

static void
init_string_format(CoilStringFormat *format)
{
  g_return_if_fail(format);

  memset(format, 0, sizeof(*format));

  format->indent_level = indent_level;
  format->block_indent = block_indent;
  format->brace_indent = brace_indent;
  format->multiline_len = multiline_length;
  format->options |= ESCAPE_QUOTES;

  if (blank_line_after_brace)
    format->options |= BLANK_LINE_AFTER_BRACE;

  if (blank_line_after_struct)
    format->options |= BLANK_LINE_AFTER_STRUCT;

  if (blank_line_after_item)
    format->options |= BLANK_LINE_AFTER_ITEM;

  if (brace_on_blank_line)
    format->options |= BRACE_ON_BLANK_LINE;

  if (list_on_blank_line)
    format->options |= LIST_ON_BLANK_LINE;

  if (commas_in_list)
    format->options |= COMMAS_IN_LIST;

  if (no_string_quotes)
    format->options |= DONT_QUOTE_STRINGS;

  if (flatten)
    format->options |= FLATTEN_PATHS;

  if (compact)
    format->options |= COMPACT;

  if (compat)
    format->options |= LEGACY;

  if (expand_all)
    format->options |= FORCE_EXPAND;
}

static gboolean
print_dependency(GNode   *node,
                 gpointer data)
{
  g_return_val_if_fail(node, TRUE);
  g_return_val_if_fail(data, TRUE);

  CoilStringFormat *format = (CoilStringFormat *)data;
  CoilInclude      *object = COIL_INCLUDE(node->data);
  const GValue     *filepath_value;
  gchar             indent[80], *filepath;
  guint             depth, indent_len;
  GError           *error = NULL;

  if (G_NODE_IS_ROOT(node))
    return FALSE;

  depth = g_node_depth(node);

  g_object_get(object,
               "filepath_value", &filepath_value,
               NULL);

  filepath = coil_value_to_string(filepath_value, format, &error);

  if (G_UNLIKELY(error))
  {
    g_warning("Unexpected Error: %s", error->message);
    return FALSE;
  }

  indent_len = MIN(2 * (depth - 2), sizeof(indent));
  memset(&indent, ' ', indent_len);
  indent[indent_len] = '\0';

  g_printerr("%s%s%s\n",
             indent,
             (depth > 2) ? "| " : "",
             filepath);

  g_free(filepath);

  return FALSE;
}

static void
print_dependency_tree(const gchar *srcfile,
                      GNode       *tree)
{
  g_return_if_fail(srcfile);
  g_return_if_fail(tree);

  CoilStringFormat format = default_string_format;
  format.options |= DONT_QUOTE_STRINGS;

  g_printerr("---------------------------------------------------\n");
  g_printerr("Dependencies for %s:\n", srcfile);

  if (G_NODE_IS_LEAF(tree))
  {
    g_printerr("None\n");
    return;
  }

  g_node_traverse(tree,
                  G_PRE_ORDER,
                  G_TRAVERSE_ALL,
                  -1,
                  print_dependency,
                  &format);
}

static void
print_files(void)
{
  CoilStringFormat format;
  CoilStruct      *attrs, **nodes;
  gboolean         overwrite = !no_clobber_attributes;
  GString         *buffer = g_string_sized_new(8192);
  GError          *error = NULL;
  guint            i, nnodes;

  init_string_format(&format);
  attrs = parse_attributes(&error);

  if (G_UNLIKELY(error))
    goto error;

  nnodes = g_strv_length(files);
  nodes = g_new0(CoilStruct *, nnodes);

  for (i = 0; i < nnodes; i++)
  {
    if (strcmp(files[i], "-") == 0)
      nodes[i] = coil_parse_stream(stdin, NULL, &error);
    else
      nodes[i] = coil_parse_file(files[i], &error);

    if (G_UNLIKELY(error))
    {
      if (!permissive)
        goto error;

      g_printerr("Continuing despite errors: %s", error->message);
      g_error_free(error);
      error = NULL;

      if (nodes[i] == NULL)
        continue;
    }
  }

  if (merge_files)
  {
    CoilStruct *root = coil_struct_new(NULL, NULL);

    for (i = 0; i < nnodes; i++)
      if (nodes[i] && !coil_struct_merge(nodes[i], root, &error))
      {
        g_object_unref(root);
        goto error;
      }

    if (attrs
      && !coil_struct_merge_full(attrs, root, overwrite, FALSE, &error))
    {
      g_object_unref(root);
      goto error;
    }

    if (expand_all
      && !coil_struct_expand_items(root, TRUE, &error))
      goto error;

    print_struct(root, buffer, &format, &error);
  }
  else
    for (i = 0; i < nnodes; i++)
    {
      if (attrs && nodes[i]
        && !coil_struct_merge_full(attrs, nodes[i], overwrite, FALSE, &error))
        goto error;

      if (expand_all
        && !coil_struct_expand_items(nodes[i], TRUE, &error))
        goto error;

      print_struct(nodes[i], buffer, &format, &error);
      g_string_truncate(buffer, 0);
    }

  if (G_UNLIKELY(error))
    goto error;

  if (show_dependencies)
    for (i = 0; i < nnodes; i++)
    {
      GNode *tree;

      if (nodes[i] == NULL)
        continue;

      tree = coil_struct_dependency_tree(nodes[i],
                                         1, COIL_TYPE_INCLUDE,
                                         &error);

      if (G_UNLIKELY(error))
      {
        g_node_destroy(tree);
        goto error;
      }

      print_dependency_tree(files[i], tree);
      g_node_destroy(tree);
    }

  if (attrs)
    g_object_unref(attrs);

#define FREE_NODES(array, n) \
  G_STMT_START { \
  for (i = 0; i < n; i++) \
    if (array[i]) g_object_unref(array[i]); \
  g_free(array); \
  } G_STMT_END

  FREE_NODES(nodes, nnodes);

  g_string_free(buffer, TRUE);

  return;

error:
  if (attrs)
    g_object_unref(attrs);

  FREE_NODES(nodes, nnodes);

  if (error)
  {
    g_printerr("%s\n", error->message);
    g_error_free(error);
  }

  g_string_free(buffer, TRUE);
  exit(EXIT_FAILURE);
}

#undef FREE_NODES

int
main (int argc,
      char **argv)
{
  parse_options(&argc, &argv);

  if (show_version) {
      printf("coildump v%s\n"
             "Copyright (C) 2011 John O'Connor\n",
             PACKAGE_VERSION);
      exit (EXIT_SUCCESS);
  }

  coil_init();

  print_files();

  exit (EXIT_SUCCESS);
}

