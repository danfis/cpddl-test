;; Synthetic task where the metric fluent (f) is only decreased, but the
;; decrement (g ?b) is a static fluent with all initial values
;; non-positive, so the metric provably never decreases.
;; pddlCompileMetricIntoActionCosts() must compile it with the action
;; cost (- 0 (g ?b)).
(define (domain num-metric-negcoef)
  (:requirements :strips :typing :numeric-fluents)
  (:types loc)
  (:predicates (at ?l - loc) (conn ?a - loc ?b - loc))
  (:functions (f) (g ?l - loc))

  (:action move
    :parameters (?a - loc ?b - loc)
    :precondition (and (at ?a) (conn ?a ?b))
    :effect (and (not (at ?a)) (at ?b)
                 (decrease (f) (g ?b)))
  )
)
