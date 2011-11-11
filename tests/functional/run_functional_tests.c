/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#include <string.h>

#include "coil.h"

#define TEST_CASES_PATH "./cases/"
#define TEST_FILE_PREFIX "test_"
#define TEST_FILE_SUFFIX ".coil"
#define TEST_KEY_NAME "test"
#define TEST_PASS_STR "pass"
#define TEST_FAIL_STR "fail"
#define EXPECTED_KEY_NAME "expected"

static void run_test(gconstpointer arg);

static GSList *
read_test_dir(const gchar *dirpath)
{
    GDir *dir;
    GError *error = NULL;
    GSList *entries = NULL;
    const gchar *entry;

    dir = g_dir_open(dirpath, 0, &error);
    g_assert_no_error(error);

    while ((entry = g_dir_read_name(dir)) != NULL) {
        gchar *fullpath = g_build_filename(dirpath, entry, NULL);
        if (g_file_test(fullpath, G_FILE_TEST_IS_DIR)) {
            entries = g_slist_concat(entries, read_test_dir(fullpath));
        }
        else if (g_str_has_prefix(entry, TEST_FILE_PREFIX) &&
                g_str_has_suffix(entry, TEST_FILE_SUFFIX) &&
                g_file_test(fullpath, G_FILE_TEST_IS_REGULAR)) {
            entries = g_slist_prepend(entries, fullpath);
        }
        else {
            g_free(fullpath);
        }
    }
    g_dir_close(dir);
    return entries;
}

static void
build_functional_test_suite(void)
{
    GSList *list = read_test_dir(TEST_CASES_PATH);

    if (list == NULL) {
        g_error("No test cases found in %s", TEST_CASES_PATH);
    }
    while (list) {
        gchar *testname = g_strconcat("/", list->data, NULL);
        g_test_add_data_func(testname, list->data, run_test);
        list = g_slist_next(list);
    }
    g_slist_free(list);
}

static void
expect_pass(const gchar *filepath)
{
    CoilObject *root;
    GError *error = NULL;
    const GValue *test, *expected;

    root = coil_parse_file(filepath, &error);
    g_assert_no_error(error);

    test = coil_struct_lookup(root, TEST_KEY_NAME,
            sizeof(TEST_KEY_NAME)-1, FALSE, &error);
    g_assert_no_error(error);

    expected = coil_struct_lookup(root, EXPECTED_KEY_NAME,
            sizeof(EXPECTED_KEY_NAME)-1, FALSE, &error);
    g_assert_no_error(error);

    if (test == NULL && expected == NULL) {
        /* just check syntax
         * expand everything to catch expand errors
         */
        coil_struct_expand_items(root, TRUE, &error);
        g_assert_no_error(error);
    }
    else if (test == NULL) {
        g_error("Must specify '%s' in coil pass test file.", TEST_KEY_NAME);
    }
    else if (expected == NULL) {
        g_error("Must specify '%s' in coil pass test file.", EXPECTED_KEY_NAME);
    }
    else {
        gint result = coil_value_compare(test, expected, &error);
        g_assert_no_error(error);

        if (result) {
            gchar *string;
            CoilStringFormat format = default_string_format;

            format.options |= FORCE_EXPAND;

            g_assert_no_error(error);
            string = coil_object_to_string(root, &format, &error);
            g_assert_no_error(error);

            g_print("Failed: \n\n%s\n", string);
            g_free(string);
        }
        g_assert_cmpint(result, ==, 0);
    }
    coil_object_unref(root);
}

static void
expect_fail(const gchar *filepath)
{
    CoilObject *root;
    GError *error = NULL;

    root = coil_parse_file(filepath, &error);
    /* expand file to catch intentional expand errors */
    /* TODO: add specific error checking */
    if (root && error == NULL) {
        coil_struct_expand_items(root, TRUE, &error);
    }
    g_assert(error != NULL);
}

static void
run_test(const void *arg)
{
    gchar *filepath = (gchar *)arg;
    guint offset = sizeof(TEST_CASES_PATH)-1;

    if (!strncmp(filepath + offset, TEST_PASS_STR, sizeof(TEST_PASS_STR)-1)) {
        expect_pass(filepath);
    }
    else if (!strncmp(filepath + offset, TEST_FAIL_STR, sizeof(TEST_FAIL_STR)-1)) {
        expect_fail(filepath);
    }
    else {
        g_warning("Ignoring file '%s'", filepath);
    }
    g_free(filepath);
}

int main(int argc, char **argv)
{
    g_type_init();
    g_test_init(&argc, &argv, NULL);

    build_functional_test_suite();

    return g_test_run();
}

