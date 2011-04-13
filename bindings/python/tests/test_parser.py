#!/usr/bin/env python

import os

from unittest import TestCase
from common import cCoil

class TestParser(TestCase):

  SIMPLE_FILE = os.path.join(os.path.dirname(__file__), 'simple.coil')

  def testEmpty(self):
    root = cCoil.parse('')
    self.assertTrue(isinstance(root, cCoil.Struct))
    self.assertEquals(len(root), 0)

  def testSingle(self):
    root = cCoil.parse('this: "that"')
    self.assertEquals(len(root), 1)
    self.assertEquals(root['this'], 'that')

  def testMany(self):
    root = cCoil.parse('this: "that" int: 1 float: 2.0')
    self.assertEquals(len(root), 3)
    self.assertEquals(root['this'], 'that')
    self.assert_(isinstance(root['int'], long))
    self.assertEquals(root['int'], 1)
    self.assert_(isinstance(root['float'], float))
    self.assertEquals(root['float'], 2.0)

  def testStruct(self):
    root = cCoil.parse('foo: { bar: "baz" } -moo: "cow"')
    self.assert_(isinstance(root['foo'], cCoil.Struct))
    self.assertEquals(root['foo']['bar'], 'baz')
    self.assertEquals(root.get('foo.bar'), 'baz')
    self.assertEquals(root.get('@root.foo.bar'), 'baz')
    self.assertEquals(root['-moo'], 'cow')

  def testOldExtends(self):
    root = cCoil.parse('a: { x: "x" } b: { @extends: ..a }')
    self.assertEquals(root['b']['x'], 'x')

  def testNewExtends(self):
    a = cCoil.parse('a: { x: "x" } b: a{}')
    b = cCoil.parse('a.x: "x" b: a{}')
    self.assertEquals(a['b']['x'], 'x')
    self.assertEquals(b['b']['x'], 'x')
    self.assertEquals(a, b)

  def testReferences(self):
    root = cCoil.parse('a: "a" b: a x: { c: ..a d: =..a }')
    self.assertEquals(root['a'], 'a')
    self.assertEquals(root['b'], 'a')
    self.assertEquals(root.get('x.c'), 'a')
    self.assertEquals(root.get('x.d'), 'a')

  def testDelete(self):
    buf = '''
    a: { x: 'x' y: 'y' }
    b: a { ~y }
    '''
    root = cCoil.parse(buf)
    self.assertEquals(root['b.x'], 'x')
    self.assertEquals(root['b']['x'], 'x')
    self.assertRaises(cCoil.KeyMissingError, lambda: root['b.y'])
    self.assertRaises(cCoil.KeyMissingError, lambda: root['b']['y'])
    self.assertEquals(len(root), 2)
    self.assertEquals(len(root['a']), 2)
    self.assertEquals(len(root['b']), 1)

  def testFile(self):
    root = cCoil.parse("@file: %s" % repr(self.SIMPLE_FILE))
    self.assertEquals(root.get('x'), 'x value')
    self.assertEquals(root.get('y.z'), 'z value')

  def testFileSub(self):
    root = cCoil.parse('sub: { @file: [%s "y"] }' % repr(self.SIMPLE_FILE))
    self.assertEquals(root.get('sub.z'), 'z value')

#    self.assertRaises(cCoil.StructError, cCoil.parse,
#        'sub: { @file: [%s "a"] }' % repr(path))
#
#    self.assertRaises(cCoil.StructError, cCoil.parse,
#        'sub: { @file: [%s "x"] }' % repr(path))

  def testFileDelete(self):
    root = cCoil.parse('sub: { @file: %s ~y.z }' % repr(self.SIMPLE_FILE))
    self.assertEquals(root.get('sub.x'), 'x value')
    self.assert_(root.get('sub.y', None) is not None)
    self.assertRaises(KeyError, lambda: root.get('sub.y.z'))

#  def testFileExpansion(self):
#    buf = '''
#    path: %s
#    sub: { @file: '${@root.path}' }"
#    ''' % repr(self.SIMPLE_FILE)
#    root = cCoil.parse(buf)
#    self.assertEqual(root.get('sub.x'), 'x value')
#    self.assertEqual(root.get('sub.y.z'), 'z value')

  def testPackage(self):
    root = cCoil.parse('@package: "coil.test:simple.coil"')
    self.assertEquals(root.get('x'), 'x value')
    self.assertEquals(root.get('y.z'), 'z value')

  def testComments(self):
    root = cCoil.parse('y: [12 #hello\n]')
    self.assertEquals(root['y'], [12])

  def testParseError(self):
    for coil in (
        'struct: {',
        'struct: }',
        'a: b:',
        ':',
        '[]',
        'a: ~b',
        '@x: 2',
        'x: 12c',
        'x: 12.c3',
        'x: @root',
        'x: { @package: "coil.test:nosuchfile" }',
        'x: { @package: "coil.test:test_parser.py"}',
        'z: [{x: 2}]',
        r'z: "lalalal \"',
        'a: [1 2 3]]',
        ):
      self.assertRaises(cCoil.CoilError, cCoil.parse, coil)

  def testOrder(self):
    self.assertEqual(cCoil.parse('x: =y y: "foo"')['x'], 'foo')
    self.assertEqual(cCoil.parse('y: "foo" x: y')['x'], 'foo')


  def testList(self):
    root = cCoil.parse('x: ["a" 1 2.0 True False None]')
    self.assertEqual(root['x'], ['a', 1, 2.0, True, False, None])

  def testNestedList(self):
    root = cCoil.parse('x: ["a" ["b" "c"]]')
    self.assertEqual(root['x'], ['a', ['b', 'c']])

  def testEquality(self):
    a = cCoil.parse('a:1 b:2')
    b = cCoil.parse('a:1 b:2')
    c = cCoil.parse('a:2 b:2')
    d = cCoil.parse('a:2 b:1')
    self.assertEquals(a, b)
    self.assertEquals(b, a)
    self.assertNotEqual(a, c)
    self.assertNotEqual(a, d)
    self.assertNotEqual(c, d)
    self.assertNotEqual(b, c)
    self.assertNotEqual(b, d)


class ExtendsTestCase(TestCase):

  def setUp(self):
    self.root = cCoil.parse('''
      A: {
          a: 'a'
          b: 'b'
          c: 'c'
      }
      B: A {
        e: [ 'one' 2 'three' ]
        ~c
      }
      C: {
        a: ..A.a
        b: @root.B.b
      }
      D: B {}
      E: {
        F.G.H: {
          a:1 b:2 c:3
        }
        F.G.I: {
          @extends: ..H
        }
      }
    ''')

  def testBasic(self):
    self.assertEquals(self.root['A.a'], 'a')
    self.assertEquals(self.root['A.b'], 'b')
    self.assertEquals(self.root['A.c'], 'c')
    self.assertEquals(len(self.root['A']), 3)

  def testExtendsAndDelete(self):
    self.assertEquals(self.root['B']['a'], 'a')
    self.assertEquals(self.root['B.b'], 'b')
    self.assertRaises(KeyError, lambda: self.root['B']['c'])
    self.assertEquals(self.root['B']['e'], ['one', 2, 'three'])
    self.assertEquals(len(self.root['B']), 3)

  def testReference(self):
    self.assertEquals(self.root['C.a'], 'a')
    self.assertEquals(self.root['C.b'], 'b')
    self.assertEquals(len(self.root['C']), 2)

  def testExtends(self):
    self.assertEquals(self.root['D.a'], 'a')
    self.assertEquals(self.root['D.b'], 'b')
    self.assertRaises(KeyError, lambda: self.root['D.c'])
    self.assertEquals(self.root['D.e'], ['one', 2, 'three'])
    self.assertEquals(len(self.root['D']), 3)

  def testRelativePaths(self):
    self.assertEquals(self.root['E.F.G.H.a'], 1)
    self.assertEquals(self.root['E.F.G.I.a'], 1)
    self.assertEquals(self.root['E.F.G.H'], self.root['E.F.G.I'])


class ParseFileTestCase(TestCase):

  def setUp(self):
    self.path = os.path.dirname(__file__)


  def testExample(self):
    root = cCoil.parse_file(os.path.join(self.path, "example.coil"))
    self.assertEquals(root['x'], 1)
    self.assertEquals(root.get('y.a'), 2)
    self.assertEquals(root.get('y.x'), 1)
    self.assertEquals(root.get('y.a2'), 2)
    self.assertEquals(root.get('y.x2'), 1)
    self.assertEquals(root.get('y.x3'), '1')

  def testExample2(self):
    root = cCoil.parse_file(os.path.join(self.path, "example2.coil"))
    self.assertEquals(root.get('sub.x'), "foo")
    self.assertEquals(root.get('sub.y.a'), "bar")
    self.assertEquals(root.get('sub.y.x'), "foo")
    self.assertEquals(root.get('sub.y.a2'), "bar")
    self.assertEquals(root.get('sub.y.x2'), "foo")
    self.assertEquals(root.get('sub.y.x3'), "foo")
    self.assertEquals(root.get('sub2.y.a'), 2)
    self.assertEquals(root.get('sub2.y.x'), 1)
    self.assertEquals(root.get('sub2.y.a2'), 2)
    self.assertEquals(root.get('sub2.y.x2'), 1)
    self.assertEquals(root.get('sub2.y.x3'), "1")
    self.assertEquals(root.get('sub3.y.a'), "bar")
    self.assertEquals(root.get('sub3.y.x'), "zoink")
    self.assertEquals(root.get('sub3.y.a2'), "bar")
    self.assertEquals(root.get('sub3.y.x2'), "zoink")
    self.assertEquals(root.get('sub3.y.x3'), "zoink")

  def testExample3(self):
    root = cCoil.parse_file(os.path.join(self.path, "example3.coil"))
    self.assertEquals(root['x'], 1)
    self.assertEquals(root.get('y.a'), 2)
    self.assertEquals(root.get('y.x'), 1)
    self.assertEquals(root.get('y.a2'), 2)
    self.assertEquals(root.get('y.b'), 3)


#class MapTestCase(TestCase):
#
#  def setUp(self):
#      self.root = cCoil.parse('''
#          expanded: {
#              a1: {
#                  z: 1
#                  x: 1
#                  y: 1
#              }
#              a2: {
#                  z: 1
#                  x: 2
#                  y: 3
#              }
#              a3: {
#                  z: 1
#                  x: 3
#                  y: 5
#              }
#              b1: {
#                  z: 2
#                  x: 1
#                  y: 1
#              }
#              b2: {
#                  z: 2
#                  x: 2
#                  y: 3
#              }
#              b3: {
#                  z: 2
#                  x: 3
#                  y: 5
#              }
#          }
#          map: {
#              @map: [1 2 3]
#              x: [1 2 3]
#              y: [1 3 5]
#              a: { z: 1 }
#              b: { z: 2 }
#          }
#          map1: {
#              @extends: ..map
#          }
#          map2: {
#              @extends: ..map
#              a: { z: 3 }
#              j: [7 8 9]
#          }
#          ''')
#
#
#  def testMap(self):
#      self.assertEquals(self.root['map'], self.root['expanded'])
#
#  def testExtends(self):
#      self.assertEquals(self.root['map1'], self.root['expanded'])
#      self.assertEquals(self.root['map2.a1.z'], 3)
#      self.assertEquals(self.root['map2.a1.j'], 7)
#      self.assertEquals(self.root['map2.a2.z'], 3)
#      self.assertEquals(self.root['map2.a2.j'], 8)
#      self.assertEquals(self.root['map2.a3.z'], 3)
#      self.assertEquals(self.root['map2.a3.j'], 9)

class ReparseTestCase(TestCase):

  def testStringWhitespace(self):
      text = """a: 'this\nis\r\na\tstring\n\r\n\t'"""
      orig = cCoil.parse([text]).root()
      new = cCoil.parse([str(orig)]).root()
      self.assertEquals(orig, new)

# TODO(jcon): move to struct expand tests
#        'x: ..a',
#        'a: { @extends: @root.b }',
#        'a: { @extends: ..b }',
#        'a: { @extends: x }',
#        'a: { @extends: . }',
#        'a: 1 b: { @extends: ..a }',
#        'a: { @extends: ..a }',
#        'a: { b: {} @extends: b }',
#        'a: { b: { @extends: ...a } }',
