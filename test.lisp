(eval
    (print "What is your age?\nAge: ")
    (let x (parseint (readline)))
    (println "I think you are" x (if (- x 1) "years" "year") "old.")
)
