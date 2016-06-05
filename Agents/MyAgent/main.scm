;HW4 - Natural Selection
;Augustus Boling
;COEN 166
;8 June 2016

;-----------------------
;  PERMANENT VARIABLES
;-----------------------
(define last_position '(0 0))
(define frontier '())

;-----------------
;  API FUNCTIONS
;-----------------
(define (initialize-agent) "MORITURI TE SALUTANT!") ;Because why not...

;For now, moves forward 1 space if it can. If it can't, then Agent turns to the right.
(define (choose-action current_energy previous_events percept)
  (if (not (equal? (get-move1 percept) 'empty)) "TURN-RIGHT"
    (begin
      (if (equal? (car previous_events) '(moved 1))
        (set! last_position ((car last_position) (+ (car (cdr last_position)) 1)))
        (set! last_position last_position)
      )
      (display last_position)
      ("MOVE-PASSIVE-1")
    )
  )
)



;------------------------------
;  TARGET-BASED A* NAVIGATION
;------------------------------

;DEV_NOTE: Here's my $0.02 on this function: this will take an arbitrary coordinate pair, representing a vegetation square on the map
;as an argument. It will return an action string representing the first step of the most optimal path to get there. Zooming out to big
;picture here, if used with a function that regularly checks for the best target, then this function will be regularly recalculating
;the route to the best target, and taking the relevant step in reaching it.
;
;Here's what still needs to be done. First it has to get an updated copy of the frontier, which required get-frontier() to be finished.
;Then it needs two algorithms for calculating X and Y deltas for the target relative to the agent, which will then inform a series of moves
;and turns to resolve to (0,+-1) or (+-1, 0). At this point, the target vegetation will have been reached, and the frontier will have to be "popped"
;to reflect depletion. Aformentioned frontier management may or may not be a part of this function.
(define (get-move percept previous_events)
  ("NOTHING FOR NOW!")
)

;Get-Frontier Function:
;TAKES: PERCEPT, LIST OF PREVIOUS EVENTS, OLD FRONTIER
;RETURNS: A SORTED LIST OF TARGETS, WITH EACH TARGET OF THE FORM -> (Rating# Abs_X_Coord Abs_Y_Coord Value)
;
;DEV_NOTE: Here's my thoughts on this: the percept isn't constant, so I can't use it as a constant over multiple turns. That said,
;the plants won't move (much?) so if I record their position relative to myself, and set that information as a variable,
;then I can track them over multiple turns. This makes a couple of assumptions: first that an update algorithm will be
;relatively easy to write, and second, that a simple navigation algorithm will be able to turn target data into a
;good move (assuming the target is good, so three assumptions I guess).
(define (get-frontier percept previous_events)
  (let
    (
      (current_position (get-position previous_events))
    )
  )
)


;Get-Row-Targets Function: This function takes a row of the percept, and that row's relative y-coordinate as its
;arguments. It then returns a list of the vegetation squares within that row of the percept, including their x and y coordinates,
;as well as the value of the plant.
;
;DEV_NOTE: When initiating an outside call to the function, the y-coordinate for the bottom row should be "1".
;This is because the space (0,0) is assumed to be occupied by the Agent. Values passed to y should be between [1,5], inclusive.
(define (get-row-targets row y)

		(if (not (null? (cdr row)))
			(let
				(
					(current (car row))
				)
				(cond
					((equal? 'empty current) (get-row-targets (cdr row) y))
					((equal? 'vegetation (car current)) (append (list (list (- (+ 1 y) (length row)) y (car (cdr current)))) (get-row-targets (cdr row) y)))
					(else (get-row-targets (cdr row) y))
				)
			)
			(let
				(
					(current (car row))
					(x (- (+ 1 y) (length row)))
				)
				(cond
					((equal? 'empty current) '())
					((equal? 'vegetation (car current)) (list (list x y (car (cdr current)))))
					(else '())
				)
			)
		)
)

;TAKES: A LIST OF ACTIONS PREVIOUSLY TAKEN
;RETURNS: CURRENT HEADING AND DIRECTION.
;
;DEV_NOTE: Right now I'm envisioning a convention as follows: the space the agent starts at is treated like (0,0). All move-actions are treated like
;transformations on this coordinate based on the agent's heading. The heading is similarly assumed to start facing "North", with all other compass
;directions implied from the original heading. Turn-actions will cause the heading to be adjusted accordingly. Thus, this functions just traces steps.
(define (get-position prev_even)
  ("NOTHING FOR NOW")
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
