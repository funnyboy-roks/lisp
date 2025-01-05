(eval
    (let x 1)
    (let cont true)
    (for () cont (= x (+ x 1)) (eval
        (println x)
        (= cont (- 5 x))
    ))
    (println "done " x)
)

; for (init; condition; after) body
