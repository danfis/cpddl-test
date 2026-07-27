;; Synthetic task with a float constant in the initial state and a
;; division in an action effect. pddlCompileFloatToInt() must refuse to
;; rescale it (NOT_COMPILABLE).
(define (domain num-float-div)
  (:requirements :strips :typing :numeric-fluents)
  (:types farm)
  (:predicates (adj ?f1 ?f2 - farm))
  (:functions (x ?f - farm))

  (:action move
    :parameters (?f1 ?f2 - farm)
    :precondition (and (adj ?f1 ?f2) (>= (x ?f1) 1))
    :effect (and (decrease (x ?f1) 1) (increase (x ?f2) 4))
  )

  (:action halve
    :parameters (?f - farm)
    :precondition (>= (x ?f) 1)
    :effect (assign (x ?f) (/ (x ?f) 2))
  )
)
