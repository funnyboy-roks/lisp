(eval
    (print "What is your age?\nAge: ")
    (let x (parseint (readline)))
    (println "You were born" x (if (!= x 1) "years" "year") "ago.")
)
