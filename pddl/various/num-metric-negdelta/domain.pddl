;; Synthetic task with a :metric fluent that is only decreased, i.e., the
;; change of the metric is negative.
;; pddlCompileMetricIntoActionCosts() must reject it.
(define (domain num-metric-negdelta)
  (:requirements :strips :typing :numeric-fluents)
  (:types loc)
  (:predicates (at ?l - loc) (conn ?a - loc ?b - loc))
  (:functions (f))

  (:action move
    :parameters (?a - loc ?b - loc)
    :precondition (and (at ?a) (conn ?a ?b))
    :effect (and (not (at ?a)) (at ?b)
                 (decrease (f) 1))
  )
)
