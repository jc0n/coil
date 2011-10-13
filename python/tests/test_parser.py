#!/usr/bin/env python

import os

from unittest import TestCase
from common import ccoil

class TestParser(TestCase):

  SIMPLE_FILE = os.path.join(os.path.dirname(__file__), 'simple.coil')

  def testEmpty(self):
    root = ccoil.parse('')
    self.assertTrue(isinstance(root, ccoil.Struct))
    self.assertEquals(len(root), 0)

  def testSingle(self):
    root = ccoil.parse('this: "that"')
    self.assertEquals(len(root), 1)
    self.assertEquals(root['this'], 'that')

  def testMany(self):
    root = ccoil.parse('this: "that" int: 1 float: 2.0')
    self.assertEquals(len(root), 3)
    self.assertEquals(root['this'], 'that')
    self.assert_(isinstance(root['int'], long))
    self.assertEquals(root['int'], 1)
    self.assert_(isinstance(root['float'], float))
    self.assertEquals(root['float'], 2.0)

  def testStruct(self):
    root = ccoil.parse('foo: { bar: "baz" } -moo: "cow"')
    self.assert_(isinstance(root['foo'], ccoil.Struct))
    self.assertEquals(root['foo']['bar'], 'baz')
    self.assertEquals(root.get('foo.bar'), 'baz')
    self.assertEquals(root.get('@root.foo.bar'), 'baz')
    self.assertEquals(root['-moo'], 'cow')

  def testOldExtends(self):
    root = ccoil.parse('a: { x: "x" } b: { @extends: ..a }')
    self.assertEquals(root['b']['x'], 'x')

  def testNewExtends(self):
    a = ccoil.parse('a: { x: "x" } b: a{}')
    b = ccoil.parse('a.x: "x" b: a{}')
    self.assertEquals(a['b']['x'], 'x')
    self.assertEquals(b['b']['x'], 'x')
    self.assertEquals(a, b)

  def testExtendsList(self):
    buf = '''
    a: { x:1 y:2 z: 3}
    z: { a:3 b:2 c: 1}

    m: { @extends: [..a ..z] a:1 x:3 }
    n: { @extends: ..a, ..z a:1 x:3 }
    o: a, z { a:1 x:3 }
    '''
    root = ccoil.parse(buf)

    for k, v in (('a.x', 1), ('a.y', 2), ('a.z', 3),
                 ('z.a', 3), ('z.b', 2), ('z.c', 1),
                 ('m.x', 3), ('m.a', 1), ('m.y', 2),
                 ('m.b', 2), ('m.z', 3), ('m.c', 1)):
      self.assertEquals(root[k], v)

    self.assertEquals(root['m'], root['n'])
    self.assertEquals(root['n'], root['o'])

    self.assertEquals(len(root['a']), 3)
    self.assertEquals(len(root['z']), 3)

    for k in ('m', 'n', 'o'):
      self.assertEquals(len(root[k]), 6)

  def testExtendsLink(self):
    buf = '''
    a.x: 1
    b: a
    c: b { y: 2 }
    '''
    root = ccoil.parse(buf)

    for k, v in (('a.x', 1), ('c.x', 1), ('c.y', 2)):
      self.assertEquals(root[k], v)

    self.assertEquals(len(root['a']), 1)
    self.assertEquals(len(root['c']), 2)

  def testReferences(self):
    root = ccoil.parse('a: "a" b: a x: { c: ..a d: =..a }')
    self.assertEquals(root['a'], 'a')
    self.assertEquals(root['b'], 'a')
    self.assertEquals(root.get('x.c'), 'a')
    self.assertEquals(root.get('x.d'), 'a')

  def testDelete(self):
    buf = '''
    a: { x: 'x' y: 'y' }
    b: a { ~y }
    '''
    root = ccoil.parse(buf)
    self.assertEquals(root['b.x'], 'x')
    self.assertEquals(root['b']['x'], 'x')
    self.assertRaises(ccoil.KeyMissingError, lambda: root['b.y'])
    self.assertRaises(ccoil.KeyMissingError, lambda: root['b']['y'])
    self.assertEquals(len(root), 2)
    self.assertEquals(len(root['a']), 2)
    self.assertEquals(len(root['b']), 1)

  def testDeleteSub(self):
    buf = '''
    a: { x: 123  y: { x: 123 z: '321' } }
    b: a { ~y.z y.y: 123 }
    '''
    root = ccoil.parse(buf)

    for k, v in (('a.x', 123), ('a.y.x', 123), ('a.y.z', '321'),
                 ('b.y.x', 123), ('b.y.y', 123)):
      self.assertEquals(root[k], v)

    self.assertEquals(len(root), 2)

    for k, l in (('a', 2), ('a.y', 2), ('b', 2), ('b.y', 2)):
      self.assertEquals(len(root[k]), l)

  def testFile(self):
    root = ccoil.parse("@file: %s" % repr(self.SIMPLE_FILE))
    self.assertEquals(root.get('x'), 'x value')
    self.assertEquals(root.get('y.z'), 'z value')

  def testFileSub(self):
    root = ccoil.parse('sub: { @file: [%s "y"] }' % repr(self.SIMPLE_FILE))
    self.assertEquals(root.get('sub.z'), 'z value')

#    self.assertRaises(ccoil.StructError, ccoil.parse,
#        'sub: { @file: [%s "a"] }' % repr(path))
#
#    self.assertRaises(ccoil.StructError, ccoil.parse,
#        'sub: { @file: [%s "x"] }' % repr(path))

  def testFileDelete(self):
    root = ccoil.parse('sub: { @file: %s ~y.z }' % repr(self.SIMPLE_FILE))
    self.assertEquals(root.get('sub.x'), 'x value')
    self.assert_(root.get('sub.y', None) is not None)
    self.assertRaises(KeyError, lambda: root.get('sub.y.z'))

#  def testFileExpansion(self):
#    buf = '''
#    path: %s
#    sub: { @file: '${@root.path}' }"
#    ''' % repr(self.SIMPLE_FILE)
#    root = ccoil.parse(buf)
#    self.assertEqual(root.get('sub.x'), 'x value')
#    self.assertEqual(root.get('sub.y.z'), 'z value')

#  def testPackage(self):
#    root = ccoil.parse('@package: "coil.test:simple.coil"')
#    self.assertEquals(root.get('x'), 'x value')
#    self.assertEquals(root.get('y.z'), 'z value')

  def testComments(self):
    root = ccoil.parse('y: [12 #hello\n]')
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
      self.assertRaises(ccoil.CoilError, ccoil.parse, coil)

  def testOrder(self):
    self.assertEqual(ccoil.parse('x: =y y: "foo"')['x'], 'foo')
    self.assertEqual(ccoil.parse('y: "foo" x: y')['x'], 'foo')


  def testList(self):
    root = ccoil.parse('x: ["a" 1 2.0 True False None]')
    self.assertEqual(root['x'], ['a', 1, 2.0, True, False, None])

  def testNestedList(self):
    root = ccoil.parse('x: ["a" ["b" "c"]]')
    self.assertEqual(root['x'], ['a', ['b', 'c']])

  def testEquality(self):
    a = ccoil.parse('a:1 b:2')
    b = ccoil.parse('a:1 b:2')
    c = ccoil.parse('a:2 b:2')
    d = ccoil.parse('a:2 b:1')
    self.assertEquals(a, b)
    self.assertEquals(b, a)
    self.assertNotEqual(a, c)
    self.assertNotEqual(a, d)
    self.assertNotEqual(c, d)
    self.assertNotEqual(b, c)
    self.assertNotEqual(b, d)


class ExtendsTestCase(TestCase):

  def setUp(self):
    self.root = ccoil.parse('''
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
    root = ccoil.parse_file(os.path.join(self.path, "example.coil"))
    self.assertEquals(root['x'], 1)
    self.assertEquals(root.get('y.a'), 2)
    self.assertEquals(root.get('y.x'), 1)
    self.assertEquals(root.get('y.a2'), 2)
    self.assertEquals(root.get('y.x2'), 1)
    self.assertEquals(root.get('y.x3'), '1')

  def testExample2(self):
    root = ccoil.parse_file(os.path.join(self.path, "example2.coil"))
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
    root = ccoil.parse_file(os.path.join(self.path, "example3.coil"))
    self.assertEquals(root['x'], 1)
    self.assertEquals(root.get('y.a'), 2)
    self.assertEquals(root.get('y.x'), 1)
    self.assertEquals(root.get('y.a2'), 2)
    self.assertEquals(root.get('y.b'), 3)


#class MapTestCase(TestCase):
#
#  def setUp(self):
#      self.root = ccoil.parse('''
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
      orig = ccoil.parse([text]).root()
      new = ccoil.parse([str(orig)]).root()
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
