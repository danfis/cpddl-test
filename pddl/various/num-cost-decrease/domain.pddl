;; Synthetic task where the metric fluent (total-cost) is only decreased,
;; but the decrement (h ?b) is a static fluent with all initial values
;; non-positive integers, so every change of (total-cost) is a
;; non-negative integer.
;; pddlIsMetricExpressibleAsNonNegIntActionCosts() must return true.
(define (domain num-cost-decrease)
  (:requirements :strips :typing :numeric-fluents)
  (:types loc)
  (:predicates (at ?l - loc) (conn ?a - loc ?b - loc))
  (:functions (total-cost) (h ?l - loc))

  (:action move
    :parameters (?a - loc ?b - loc)
    :precondition (and (at ?a) (conn ?a ?b))
    :effect (and (not (at ?a)) (at ?b)
                 (decrease (total-cost) (h ?b)))
  )
)
