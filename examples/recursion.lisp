(eval
    (function foo x (
        if (< x 5) (eval
            (println x)
            (foo (+ x 1))
        )
    ))
    (foo 0)
)
