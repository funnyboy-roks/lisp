(eval
    (let a (. "a" 0))
    (println "(int a) =" (int a))
    (println "(char (int a)) =" (char (int a)))
    (println "(== a (char (int a))) =" (== a (char (int a))))
)
