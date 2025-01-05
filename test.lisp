(eval
    (let x 0)
    (while (- 5 x) (eval
        (println x)
        (= x (+ x 1))
    ))
    (println "done")
)
