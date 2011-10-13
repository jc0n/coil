#!/usr/bin/env python

# Mostly from pygtk-2.16.0/tests/runtests.py
#

import glob
import os
import unittest
import sys

import common

program = None
if len(sys.argv) == 3:
  buildDir, srcDir = sys.argv[1:3]
else:
  if len(sys.argv) == 2:
    program = sys.argv[1]
    if program.endswith('.py'):
      program = program[:-3]
  buildDir, srcDir = '..', '.'

common.importModules(buildDir)

dir = os.path.split(os.path.abspath(__file__))[0]
os.chdir(dir)

def gettestnames():
  files = glob.glob('test_*.py')
  names = map(lambda x: x[:-3], files)
  return names

suite = unittest.TestSuite()
loader = unittest.TestLoader()

for name in gettestnames():
  suite.addTest(loader.loadTestsFromName(name))

testRunner = unittest.TextTestRunner(verbosity=2)
result = testRunner.run(suite)

sys.exit(0 if result.wasSuccessful() else 1)
