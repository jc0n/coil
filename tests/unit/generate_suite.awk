#!/usr/bin/awk -f
BEGIN {
  FS="[()]+"
  n = 0
  suite = ""
  pfx = "test_"
  print "#include \"coil.h\""
  print ""
  printf "#define COIL_TEST_CASE(fn) static void %s##fn(void)\n", pfx
  print "#line 2"
}

/^[ ]*COIL_TEST_SUITE/ {
  gsub("\"", "", $2)
  suite = $2
  next
}

/^[ ]*COIL_TEST_CASE/ {
  tests[n++] = $2;
}

{ print }

END {
  print ""
  print "/* Do not modify the code below it has been automatically generated"
  print " * by generate_suite.awk re-run that script instead"
  print " */"
  print "int main(int argc, char **argv)"
  print "{"
  print "  g_test_init(&argc, &argv, NULL);"
  print "  g_type_init();\n"
  for ( fn in tests )
  {
    printf("  g_test_add_func(\"/%s/%s\", %s%s);\n", suite, tests[fn],
           pfx, tests[fn]);
  }

  print "\n  return g_test_run();"
  print "}"
}

