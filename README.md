# Lisp

A simple lisp-style interpreter made by someone who has never interacted
with anything in the lisp language family.

Because of that, this is likely nothing like a "proper" lisp dialect.

The only thing close to research that I did was play around with my
editor's (NeoVim) highlighting for "`.lisp`" files and found some
keywords that were highlighted.

```lisp
(eval
    (print "What is your age?\nAge: ")
    (let x (parseint (readline)))
    (println "You were born" x (if (!= x 1) "years" "year") "ago.")
)
```

## Literals

- Strings - `'foo'` and `"foo"`
- Integers - `123`, `0b101`, `0x123`
- Boolean - `true` and `false`

## Global Functions

- `print` - print values to stdout
- `println` - `print` with trailing newline
- `parseint` - parse string to int
- `readline` - read one line from stdin
- `append` - append value to array (returns new array)
- `length` - get length of array or string
- `int`, `char`, `string`, `bool` - cast value to given type

## Operations

- `-` - Subtract `(- 1 2 3)` -> `-4`
- `+` - Add numbers / Concat strings `(+ 1 2 "hello" true)` -> `3hellotrue`
- `*` - Multiply `(* 2 3 4)` -> `24`
- `/` - Divide `(/ 10 5)` -> `2`
- `@` - Create array `(@ 1 2 3)`
- `==` - Equals
- `!=` - Not Equals
- `<`, `<=` - Less than, less than or equal to
- `>`, `>=` - Greater than, greater than or equal to
- `.` - Index into array or string `(. "hello" 0)` -> `'h'`

## Variables

```lisp
(let x)         ; declare variable x
(let y (+ 2 3)) ; declare variable x and assign to it
(= x (+ 3 10))  ; assign value to previously declared variable
(= y 1)         ; assign value to previously declared variable
```

## User Functions

```lisp
(function name param1 param2 ... body)
```

Example:

```lisp
(eval
    (function foo x (
        if (- 5 x) (eval
            (println x)
            (foo (+ x 1))
        )
    ))
    (foo 0)
)
```

## Conditionals

```lisp
(if condition (then))
; and
(if condition (then) (else))
```

## Loops

While Loops: 

```lisp
(while condition body)
```

Example:

```lisp
(let i 0)
(while (< i (length a)) (eval
    (println (+ "a[" i "] =") (. a i))
    (= i (+ i 1))
))
```

For Loops: 

```lisp
(for init condition post body)
```

Example:

```lisp
(for (let i 0) (< i (length a)) (= i (+ i 1)) (eval
    (println (+ "a[" i "] =") (. a i))
))
```
