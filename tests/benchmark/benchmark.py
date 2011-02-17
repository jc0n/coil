#!/usr/bin/env python
import sys, os
import coil

def main(args):
  for file in args[1:]:
      print coil.parse_file(file)

if __name__ == "__main__":
  main(sys.argv)
