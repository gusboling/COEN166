; Seed random number generator
; Example code from guile user's manual
; http://www.gnu.org/software/guile/manual/html_node/Random.html
(let ((time (gettimeofday)))
      (set! *random-state*
            (seed->random-state (+ (car time)
                                   (cdr time)))))

(define (initialize-agent)
        "OK")

(define (choose-action current-energy previous-events percepts)
	(let ((rand (random 12)))
             (cond ((equal? rand 0) "STAY")
                   ((equal? rand 1) "TURN-RIGHT")
                   ((equal? rand 2) "TURN-LEFT")
                   ((equal? rand 3) "TURN-AROUND")
                   ((equal? rand 4) "MOVE-PASSIVE-1")
                   ((equal? rand 5) "MOVE-PASSIVE-2")
                   ((equal? rand 6) "MOVE-PASSIVE-3")
                   ((equal? rand 7) "MOVE-AGGRESSIVE-1")
                   ((equal? rand 8) "MOVE-AGGRESSIVE-2")
                   ((equal? rand 9) "MOVE-AGGRESSIVE-3")
                   ((equal? rand 10) "EAT-PASSIVE")
                   ((equal? rand 11) "EAT-AGGRESSIVE")
                   (#f "STAY"))))

