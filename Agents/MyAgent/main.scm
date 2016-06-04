;HW4 - Natural Selection
;Augustus Boling
;COEN 166
;8 June 2016

;-----------------
;  API FUNCTIONS
;-----------------
(define (initialize-agent) "AVE IMPERATOR! MORITURI TE SALUTANT!") ;Because why not...

;For now, moves forward 1 space if it can. If it can't, then Agent turns to the right.
(define (choose-action current_energy previous_events percept)
	(begin
		(cond
			((equal? (get-move1 percept) 'empty) "MOVE-PASSIVE-1")
			(#t "TURN-RIGHT")
		)
       	)
)



;---------------------------
;  REFLEXIVE A*-NAVIGATION
;---------------------------
;This function takes the current percepts as an argument, and returns a sorted list (in ascending order)
;of the various actions that can be taken. At the moment, only passive movement and turning left or right
;is supported. More to follow.
(define (get-frontier percept)
	(let
		(
			(MOVEP1 (get-f-value percept "MOVE-PASSIVE-1"))
			(MOVEP2 (get-f-value percept "MOVE-PASSIVE-2"))
			(MOVEP3 (get-f-value percept "MOVE-PASSIVE-3"))
			(TR (get-f-value percept "TURN-RIGHT"))
			(TL (get-f-value percept "TURN-LEFT"))
		)
		(insert MOVEP1 (insert MOVEP2 (insert MOVEP3 (insert TR (insert TL)))))
	) 
)  

(define (get-f-value percept move_type)
	(cond
		((equal? move_type "MOVE-PASSIVE-1") (list move_type (+ (get-h-value percept move_type) 10)))
		((equal? move_type "MOVE-PASSIVE-2") (list move_type (+ (get-h-value percept move_type) 30)))
		((equal? move_type "MOVE-PASSIVE-3") (list move_type (+ (get-h-value percept move_type) 60)))
		((equal? move_type "TURN-RIGHT") (list move_type (+ (get-h-value percept move_type) 2)))
		((equal? move_type "TURN-LEFT") (list move_type (+ (get-h-value percept move_type) 2)))
		(#t (list "ERROR CALCULATING F_VALUE" 1000)) ;Debugging clause - Hopefully unecessary.
	)	
)

(define (get-h-value percept move_type)
	
)
(define (insert move_tuple)
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
