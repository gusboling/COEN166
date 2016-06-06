;HW4 - Natural Selection
;Augustus Boling
;COEN 166
;8 June 2016

;-----------------------
;  PERMANENT VARIABLES
;-----------------------
(define last_position '(0 0))
(define heading 'N)
(define frontier '())

;-----------------
;  API FUNCTIONS
;-----------------
(define (initialize-agent) "MORITURI TE SALUTANT!") ;Because why not...

;EXPLANATION OF BEHAIVIOR: At the beginning of each turn, the agent's first action is to take in previous events and calculate its present position.
;It then should update the frontier using the current position, heading, and percept. Next the agent evaluates the percept for several conditions which
;would induce a reflexive move - such as a edible plant being within 4 spaces or a impassible space in front of the agent. These result in a predefined 
;action that will address the conditions that gave rise to it. If none of the reflexes are triggered, then the agent uses the frontier to decide on its next
;course of action.
;
;In summary, if it can't take any immediate reflexive action, then it uses an internal model of the environment to navigate to position where the reflexes can
;take over.
(define (choose-action current_energy previous_events percept)
	(begin
		(let
			((new_loch (get-new-position previous_events last_position)))
			
			(begin
				(set! last_position (car new_loch))
				;SET NEW FRONTIER DOWN HERE.
			)
		)
		(display current_energy)
		(display "	")
		(cond
			;CONSIDER ADDING FLEEING RESPONSES FOR AGENT AND PREDATOR SIGHTING 
			((edible? (get-space1 percept)) (eat percept))
			((one_away? percept) (s-lunge))
			((two_away? percept) (m-lunge))
			((three_away? percept) (l-lunge))
			((passable? (get-space1 percept)) (s-move)) ;REPLACE WITH: A* SEARCH STEP, AS DICTATED BY (car frontier)))
			(#t (turn-right heading))
		)
	)
)

;------------------------------
;  BEHAIVIOR HELPER FUNCTIONS
;------------------------------  
(define (turn-right direction)
	(cond
		((equal? direction 'N) (begin (set! heading 'E) (display "Turned Right	") "TURN-RIGHT"))
		((equal? direction 'E) (begin (set! heading 'S) (display "Turned Right	") "TURN-RIGHT"))
		((equal? direction 'S) (begin (set! heading 'W) (display "Turned Right	") "TURN-RIGHT"))
		((equal? direction 'W) (begin (set! heading 'N) (display "Turned Right	") "TURN-RIGHT"))
	)
)

(define (turn-left direction)
	(cond
		((equal? direction 'N) (begin (set! heading 'W) (display "Turned Left	") "TURN-LEFT"))
		((equal? direction 'W) (begin (set! heading 'S) (display "Turned Left	") "TURN-LEFT"))
		((equal? direction 'S) (begin (set! heading 'E) (display "Turned Left	") "TURN-LEFT"))
		((equal? direction 'E) (begin (set! heading 'N) (display "Turned Left	") "TURN-LEFT")) 
	)
)

(define (turn-around direction)
	(cond
		((equal? direction 'N) (begin (set! heading 'S) (display "Turned Around	") "TURN-AROUND"))
		((equal? direction 'S) (begin (set! heading 'N) (display "Turned Around	") "TURN-AROUND"))
	)
)

(define (s-move) (begin (display "Small Move	") "MOVE-PASSIVE-1"))

(define (s-lunge) (begin (display "Small Lunge	") "MOVE-AGGRESSIVE-1"))

(define (m-lunge) (begin (display "Medium Lunge	") "MOVE-AGGRESSIVE-2"))

(define (l-lunge) (begin (display "Large Lunge	") "MOVE-AGGRESSIVE-3")) 

(define (stay) (begin (display "Stayed") "STAY"))

(define (eat percept) (begin (display "Ate ") (display (car (cdr (cdr (get-space1 percept))))) (display "	") "EAT-AGGRESSIVE"))



;--------------------------------
;  ENVIRONMENT HELPER FUNCTIONS
;--------------------------------
(define (edible? infront)
	(if (null? infront) #f 
		(if (not (pair? infront)) #f
			(if (and (equal? 'vegetation (car infront)) (> (car (cdr (cdr infront))) 5)) #t #f)
		)
	)
)

(define (passable? space)
	(if (equal? space 'empty) #t #f)
)

(define (one_away? percept)
	(let
		((sq1 (get-space1 percept)) (sq2 (get-space2 percept)))
		(if (and (edible? sq2) (passable? sq1)) #t #f) 
	)
)

(define (two_away? percept)
	(let
		((sq1 (get-space1 percept)) (sq2 (get-space2 percept)) (sq3 (get-space3 percept)))
		(if (and (and (edible? sq3) (passable? sq2)) (passable? sq1)) #t #f)
	)
)

(define (three_away? percept)
	(let 
		((sq1 (get-space1 percept)) (sq2 (get-space2 percept)) (sq3 (get-space3 percept)) (sq4 (get-space4 percept)))
		(if (and (and (and (edible? sq4) (passable? sq3)) (passable? sq2)) (passable? sq1)) #t #f)
	)
)



;------------------------------
;  TARGET-BASED A* NAVIGATION
;------------------------------

;TAKES: UPDATED PERCEPT, UPDATED FRONTIER, UPDATED POSITION
;RETURNS: AN ACTION-STRING REPRESENTING THE NEXT MOVE NECESSARY TO REACH THE FIRST ELEMENT IN THE FRONTIER.
(define (get-move percept new_frontier new_position new_heading)
  ("NOTHING FOR NOW!")
)

;TAKES: PERCEPT, LIST OF PREVIOUS EVENTS, OLD FRONTIER
;RETURNS: A SORTED LIST OF TARGETS, WITH EACH TARGET OF THE FORM -> (Rating# Abs_X_Coord Abs_Y_Coord Value)
;
;DEV_NOTE: Here's my thoughts on this: the percept isn't constant, so I can't use it as a constant over multiple turns. That said,
;the plants won't move (much?) so if I record their position relative to myself, and set that information as a variable,
;then I can track them over multiple turns. This makes a couple of assumptions: first that an update algorithm will be
;relatively easy to write, and second, that a simple navigation algorithm will be able to turn target data into a
;good move.
(define (get-new-frontier percept previous_events old_frontier new_heading)
  ("NOTHING FOR NOW!")
) 

;TAKES: PERCEPT
;RETURNS: A LIST OF TARGETS IN THE CURRENT PERCEPT, WITH COORDINATES RELATIVE TO THEIR POSITION WITHIN THE PERCEPT AND THE AGENT.
;
;DEV_NOTE:The coordinates this returns ARE NOT absolute coordinates suitable for navigation. They are coordinates relative to the agent,
;which can then be translated into absolute coordinates when combined with the current position and heading.
(define (get-targets percept)
	(if (null? percept) '()
		(append (get-row-targets (car percept) (- 6 (length percept))) (get-targets (cdr percept))) 
	) 
)

;TAKES: A PERCEPT ROW, THE Y-COORDINATE ASSOCIATED WITH THAT ROW
;RETURNS: A LIST OF TARGETS IN THAT ROW, EACH REPRESENTED AS AN COORDINATE PAIR RELATIVE TO THE AGENT.
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
					((equal? 'vegetation (car current)) (append (list (list (- (+ 1 y) (length row)) y (car (cdr (cdr current))))) (get-row-targets (cdr row) y)))
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
					((equal? 'vegetation (car current)) (list (list x y (car (cdr (cdr current))))))
					(else '())
				)
			)
		)
)

;TAKES: A LIST OF ACTIONS PREVIOUSLY TAKEN
;RETURNS: CURRENT HEADING AND LOCATION.
;
;DEV_NOTE: Right now I'm envisioning a convention as follows: the space the agent starts at is treated like (0,0). All move-actions are treated like
;transformations on this coordinate based on the agent's heading. The heading is similarly assumed to start facing "North", with all other compass
;directions implied from the original heading. Turn-actions will cause the heading to be adjusted accordingly. Thus, this functions just traces steps.
(define (get-new-position prev_events prev_loc)
	(if (null? prev_events) (list prev_loc heading) ;Base case - return previous location unaltered if no actions have been taken.
		(cond
			((equal? (car prev_events) '(moved 1)) (list (position-change 1 prev_loc) heading))
			((equal? (car prev_events) '(moved 2)) (list (position-change 2 prev_loc) heading))
			((equal? (car prev_events) '(moved 3)) (list (position-change 3 prev_loc) heading))
			(#t (get-new-position (cdr prev_events) prev_loc))
		)
	) 
)

;TAKES: An (x,y) coordinate-pair, and the number of spaces moved. The coordinate pair represents the starting location.
;RETURNS: A new (x,y) coordinate-pair, representing a move of <move_spaces> spaces from the starting location, in
;the present direction.
(define (position-change move_spaces prev_position)
	(cond
		((equal? heading 'N) (list (car prev_position) (+ (car (cdr prev_position)) move_spaces)))
		((equal? heading 'S) (list (car prev_position) (- (car (cdr prev_position)) move_spaces)))
		((equal? heading 'E) (list (+ (car prev_position) move_spaces) (car (cdr prev_position))))
		((equal? heading 'W) (list (- (car prev_position) move_spaces) (car (cdr prev_position)))) 
	)
)

;TAKES: CURRENT POSITION, CURRENT HEADING, LIST OF TARGETS WITH RELATIVE COORDINATES
;RETURNS: LIST OF TARGETS WITH ABSOLUTE COORDINATES.
(define (absolute-location new_position new_heading targets)
	(if (null? targets) '()
		(let
			(
				(current_x (car new_position))
				(current_y (car (cdr new_position)))
				(target_x (car (car targets)))
				(target_y (car (cdr (car targets))))
				(target_value (car (cdr (cdr (car targets)))))				
			)
			(cond
				((equal? new_heading 'N) (append (list (+ current_x target_x) (+ current_y target_y) target_value) (absolute-location new_position new_heading (cdr targets))))
				((equal? new_heading 'S) (append (list (- current_x target_x) (- current_y target_y) target_value) (absolute-location new_position new_heading (cdr targets))))
				((equal? new_heading 'E) (append (list (+ current_x target_y) (- current_y target_x) target_value) (absolute-location new_position new_heading (cdr targets))))
				((equal? new_heading 'W) (append (list (- current_x target_y) (+ current_y target_x) target_value) (absolute-location new_position new_heading (cdr targets))))	
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
(define (get-space1 percept)
	(car (cdr (car percept)))
)

;Returns the percept of the space one square in front of the agent.
(define (get-space2 percept)
	(car (cdr (cdr (car (cdr percept)))))
)

;Returns the percept of the space two squares in front of the agent.
(define (get-space3 percept)
	(car (cdr (cdr (cdr (car (cdr (cdr percept)))))))
)

;Returns the percept of the space three squares in front of the agent.
(define (get-space4 percept)
	(car (cdr (cdr (cdr (cdr (car (cdr (cdr (cdr percept)))))))))
)
