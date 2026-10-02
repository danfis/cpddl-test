;; Synthetic task with a :metric that is not linear in the metric fluents:
;; (* (f1) (f2)) where both (f1) and (f2) are increased by the action.
;; pddlCompileMetricIntoActionCosts() must reject it.
(define (domain num-metric-nonlinear)
  (:requirements :strips :typing :numeric-fluents)
  (:types loc)
  (:predicates (at ?l - loc) (conn ?a - loc ?b - loc))
  (:functions (f1) (f2))

  (:action move
    :parameters (?a - loc ?b - loc)
    :precondition (and (at ?a) (conn ?a ?b))
    :effect (and (not (at ?a)) (at ?b)
                 (increase (f1) 1)
                 (increase (f2) 2))
  )
)
