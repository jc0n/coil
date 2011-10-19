# Coil - A General Configuration Library

Coil is a simple and powerful configuration language. Think of it as object oriented key-value pairs.

    config: { something: True }
    more_config: { other_thing: False  }
    coil: { @extends: ..config, ..more_config
        is_awesome: True
        version: 0.1
    }

Coil is defined in blocks called "structs" in which key-value pairs can be
easily abstracted for inheritance by other blocks. These pairs can be overridden or
removed, linked to other values, or used in an expression. Values can be boolean,
numbers, strings, links to other values, or lists.

The authorative implementation of Coil, which is in pure Python, resides at
http://code.google.com/p/coil and is maintained by ITA Softare where
it was originally concieved.

This project is a port in C which aims to be heavily optimized and mostly
compatibile with the pure python version. This implementation also introduces
some helpful new features and syntax.

Oh, and there is also a **python extension :)**


## Coil Basics

_(this documentation is a work in progress)_

Coil consists of keys and values. Keys can contain any alphnumeric characters.

Values are:

  - None, True, or False
  - Integers
  - Floating point numbers
  - Literal strings
  - String expressions
  - Lists of values

### Command line utility

Coil comes with a command line utility called `coildump` which provides a means
to analyze or debug coil configuration.

- View an expanded configuration: `coildump --expand-all file.coil`
- View all dependencies for a file: `coildump --dependency-tree file.coil`
- Print specific values from a file: `coildump -p x.y.z -p a.b.c file.coil`
- For detailed help and more commands: `coildump --help-all`


### More basics

*Whitespace between keys and values does not matter.*

    is_ok: True
    description: "This is Coil"

is treated the same as

    is_ok: True description: "This is Coil"


Groups of key value pairs can be defined inside of a "struct" which may look
like...

    config-a: {
      is_ok: True
      extra: False
      description: "This is config a"
    }

    config-b: {
      extra: False
      is_ok: True
      description: "This is config b"
    }

Structs can be abstracted to form base groups of common key-value pairs. For the
trivial example above we could rewrite as..

    config-base: {
      extra: False
      is_ok: True
    }

    config-a: config-base {
      description: "This is config a"
    }

    config-b: config-base {
      description: "This is config b"
    }

Multiple structs can be inherited from at the same time:

    config-c: {
      another_key: "another value"
    }

    config-d: config-a, config-c {
      description: "This is config d"
    }

Keys can be deleted to prevent inheriting the entire namespace.

    config-base-extra: config-base {
      extra: True
      ~is_ok
    }

Would be equivalent to..

    config-base-extra: {
      extra: True
    }

## Keys and Paths

All values need a way to be identified by name. Coil has a simple pathing
scheme which allows any value to be accessed by a unique path. Paths can be
relative to a struct or absolute (relative to the top level root struct). A
relative path may be prefixed with a series of dots '.' which similar to .. on a
command line means to traverse up the parent hierarchy. Absolute paths are prefixed with @root

Paths can also be used to define values or add values to a struct.

    a.b.c: 123

is equivalent to

    a: {
      b: {
        c: 123
      }
    }

To add a value to a struct:

    a: {}
    a.x: 123

yeilds `a: { x: 123 }`
or equivalently `a.x: 123`


### Common Gotchas

There are a couple of cases where the path scheme can be tricky. Particularly
when using shorthand definitions such as a.b.c: =some_path. In this case
some_path is relative to a.b as it would be if you were to use the longer more
expanded notation.

a.b.c: =somepath #somepath must be in a.b

    a: {
      b: {
        somepath: 123
        c: =somepath
      }
    }


To illustrate the relative pathing in links consider the following..

    a: {
      one: 1
      b: {
        number: ..one
      }
    }

a.b.number will have the value 1

Consider another example...

    one: 1
    a: {
      b: {
        c: {
          number: ....one
        }
      }
    }

a.b.c.number will have the value 1


## File Includes

It is possible that configuration be spread across multiple files in which case
you may want to include them.

    some_config: {
        @file: 'other.coil'
    }

### Selective Includes

When including a file you can also specify the names for blocks you wish to include if you
know them ahead of time or dont wish to pollute the namespace with everything in the file.

    some_config: {
        @file: ['components.coil' 'com1' 'com2' 'com3']
    }

The file include can also resolve references to other values. If you wanted to
store the list of imports in a list you could do so.

    some_config: {
        component_config: 'components.coil'
        installed_components: ['com1', 'com2', 'com3']
        @file: [ =component_config =installed_components ]
    }

An example of several concepts together

    component_conf: 'components.coil'
    installed_components: ['com1' 'com2' 'com3']
    components: { @file: =..component_conf }

    base_config: {
        # installed components
        com1: ..components.com1
        com2: ..components.com2
        com3: ..components.com3
    }

    prod_config: base_config {
            env: 'prod'
                ~com1 # disable component 1 in production
    }
    test_config: base_config {
            env: 'test'
            com3.disabled: True
            com3.disabled_reason: "Who knows"
    }

## Expressions

Expressions can be used to compose a value from other values.

    config-base: {
        name: 'default'
        description: "This is config ${name}"
    }
    config-a: config-base {
        name: 'a'
    }
    config-b: config-base {
        name: 'b'
    }
    config-c: config-base {
        name: 'c'
    }


## Struct Prototypes

Additions to structs after they have been defined do not affect the consistency
of inheriting structs as they will inherit the struct exactly as it is defined
*when it is inherited*.

    a.x: 1
    b: a {}
    a.y: 2

Will produce..

    a: { x: 1 y: 2 }
    b: { x: 1 }

Yet

    a.x: 1
    a.y: 2
    b: a {}

Will produce...

    a: { x:1 y:2 }
    b: { x:1 y:2 }


## Value References

Values can be linked together in any fasion

    a: 123
    b: a
    c: @root.b
    d: { e: ..c }

is the same as

    a: 123
    b: 123
    c: 123
    d.e: 123


## Changes from pure python implementation

### Added support for comma-separated list syntax:

    abc: [ 1 2 3 ] # compatible
    abc: [ 1, 2, 3 ] # new

### Added new syntax for inheritance:

##### Old Syntax

    abc: {
      @extends: ..x
      @extends: ..y
      @extends: ..z
    }

##### New Shorthand Syntax

    abc: { @extends: ..x, ..y, ..z }

or

    abc: x, y, z {}



### Dynamic Expansion

Values are expanded internally only when they are needed which allows
for decoupling the namespace (the keys) from their values until they are
accessed.

### Late Binding

    a: =b  # link to b
    b: 1

is now equivalent to

    b: 1
    a: =b

    A: { @extends: ..B } # B is defined later
    B: { x: 1 y: 2 z: 3 }


