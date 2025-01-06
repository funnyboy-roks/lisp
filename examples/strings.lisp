(eval
    (let a "hello")
    (for (let i 0) (< i (length a)) (= i (+ i 1)) (eval
        (println (+ "a[" i "] =") (. a i))
    ))
    (let b "")
    (for (let i 0) (< i (length a)) (= i (+ i 1)) (eval
        (if i (= b (+ b "-")))
        (= b (+ b (. a i)))
    ))
    (println b)
)
