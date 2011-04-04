#!/usr/bin/env python

from unittest import TestCase

from common import cCoil
Struct = cCoil.Struct

class TestStruct(TestCase):

  def setUp(self):
    self.data = (('first', {
                    'string': 'something',
                    'float': 2.5,
                    'int': 1,
                    'dict': {
                      'x': 1,
                      'y': 2,
                      'z': 'another_thing'}
                    }),
                  ('second', 'something_else'),
                  ('last', ['list', 'of', 'strings']))
#    self.struct = Struct(self.data)
    self.empty = Struct()

  def testRoot(self):
    s = self.empty
    self.assertTrue(isinstance(s, Struct))
    self.assertEqual(s.path(), '@root')
    self.assertEqual(len(s), 0)

  def testSet(self):
    s = self.empty
    s['a.b.c'], s['x.y.z'] = (123, 'test')
    self.assertEquals(len(s), 2)
    for k in ('a', 'a.b', 'a.b.c', 'x', 'x.y', 'x.y.z'):
      self.assertTrue(k in s, '%s in %s' % (k, s.path()))
      self.assertTrue(s.has_key(k))

    self.assertEquals(s['a.b.c'], 123)
    self.assertEquals(s['x.y.z'], 'test')
