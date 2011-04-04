#!/usr/bin/env python

import sys

def importModules(buildDir):
  sys.path.insert(0, buildDir)

  cCoil = importModule("cCoil")

  globals().update(locals())

def importModule(module):
  try:
    obj = __import__(module)
  except ImportError:
    raise

  return obj

