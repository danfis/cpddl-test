(define (domain cyctest)
  (:requirements :strips :negative-preconditions :conditional-effects)
  (:predicates (a) (b) (x) (y) (g))
  (:action cyc
    :parameters ()
    :precondition (and)
    :effect (and (when (a) (and (not (b)) (x)))
                 (when (b) (and (not (a)) (y)))))
  (:action finish
    :parameters ()
    :precondition (and (x) (y) (not (a)) (not (b)))
    :effect (and (g))))
