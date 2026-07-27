;; Synthetic task with float constants both in additive positions (init,
;; effects would stay integer here) and as multiplicative coefficients in
;; the goal. pddlCompileFloatToInt() must rescale (x ?f) with exponent 1
;; and the goal constant with exponent 2 (factor 10).
(define (domain num-float-coef)
  (:requirements :strips :typing :numeric-fluents)
  (:types farm)
  (:predicates (adj ?f1 ?f2 - farm))
  (:functions (x ?f - farm))

  (:action move
    :parameters (?f1 ?f2 - farm)
    :precondition (and (adj ?f1 ?f2) (>= (x ?f1) 1))
    :effect (and (decrease (x ?f1) 1) (increase (x ?f2) 4))
  )
)
