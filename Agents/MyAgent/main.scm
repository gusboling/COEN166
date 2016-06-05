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
;This function returns a list of different target-squares (just vegetation squares for now, but possibly expanded later).
;The list is sorted from most to least desirable, meaning that (car (get-frontier ... )) returns the details of the best
;target. Desirability is a function of A) square type, B) proximity to predators, C) distance to square, and D) proximity
;to other agents - other factors may be included later, but for now this is it.
;
;Here's my thoughts on this: the percept isn't constant, so I can't use it as a constant over multiple turns. That said,
;the plants won't move (much?) so if I record their position relative to myself, and set that information as a variable,
;then I can track them over multiple turns. This makes a couple of assumptions: first that an update algorithm will be
;relatively easy to write, and second, that a simple navigation algorithm will be able to turn target data into a
;good move (assuming the target is good, so three assumptions I guess).
(define (get-frontier percept previous_events frontier) ("NOTHING YET"))

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
