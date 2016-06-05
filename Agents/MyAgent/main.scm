;HW4 - Natural Selection
;Augustus Boling
;COEN 166
;8 June 2016

;-----------------
;  API FUNCTIONS
;-----------------
(define (initialize-agent)
 	(begin
		(define frontier '())
		"MORITURI TE SALUTANT!"
	)
) ;Because why not...

;For now, moves forward 1 space if it can. If it can't, then Agent turns to the right.
(define (choose-action current_energy previous_events percept)
	(begin
		(cond
			((equal? (get-move1 percept) 'empty) "MOVE-PASSIVE-1")
			(#t "TURN-RIGHT")
		)
  )
)



;------------------------------
;  TARGET-BASED A* NAVIGATION
;------------------------------
;This function takes the current percept as an argument, and returns a frontier.
;The frontier is a list of tuples, with each tuple representing a vegetation square's
;coordinates relative to the agent. The frontier list is sorted according to the hueristic
;value of each vegetation square, plus the cost of reaching it.
(define (get-frontier percept frontier previous_events)) #f)

(define (get-row-targets row y)
	(if (null? row)
		'()
		(let
			(
				(current (car row))
				(x (- 6 (length row)))
			)
			(cond
				((equal? 'empty current) (row-targets (cdr row) y))
				((equal? 'vegetation (car current)) (append (list x y (h-value x y ()))))
        (#t (row-targets (cdr row) y))
			)
		)
	)
)



;--------------------------
;  MISC. HELPER FUNCTIONS
;--------------------------
;Prints out the Agent's current percept.
(define (print-percept percept)
	(begin
		(display (car percept))
    (display "\n")
    (display (car (cdr percept)))
		(display "\n")
    (display (car (cdr (cdr percept))))
		(display "\n")
    (display (car (cdr (cdr (cdr percept)))))
		(display "\n")
		(display (car (cdr (cdr (cdr (cdr percept))))))
		(display "\n")
    #t
	)
)

;Returns the percept of the space directly in front of the agent.
(define (get-move1 percept)
	(car (cdr (car percept)))
)

;Returns the percept of the space one square in front of the agent.
(define (get-move2 percept)
	(car (cdr (cdr (car (cdr percept)))))
)

;Returns the percept of the space two squares in front of the agent.
(define (get-move3 percept)
	(car (cdr (cdr (cdr (car (cdr (cdr percept)))))))
)
