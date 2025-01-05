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
    (println "I think you are" x (if (- x 1) "years" "year") "old.")
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

## Operations

- `-` - Subtract `(- 1 2 3)` -> `-4`
- `+` - Add numbers / Concat strings `(+ 1 2 "hello" true)` -> `3hellotrue`
- `*` - Multiply `(* 2 3 4)` -> `24`
- `/` - Divide `(/ 10 5)` -> `2`

## Variables

```lisp
(let x)         ; declare variable x
(let y (+ 2 3)) ; declare variable x and assign to it
(= x (+ 3 10))  ; assign value to previously declared variable
(= y 1)         ; assign value to previously declared variable
```

## User Functions

- `(function name param1 param2 ... (body))`

### Example

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
