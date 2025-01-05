(eval
    (function foo x (
        if (- 5 x) (eval
            (println x)
            (foo (+ x 1))
        )
    ))
    (foo 0)
)
