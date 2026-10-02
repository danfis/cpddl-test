;; Synthetic task where the metric fluent is a 0-ary function that is not
;; named total-cost but behaves exactly as a total-cost accumulator.
;; pddlIsMetricExpressibleAsNonNegIntActionCosts() must return true.
(define (domain num-cost-renamed)
  (:requirements :strips :typing :numeric-fluents)
  (:types loc)
  (:predicates (at ?l - loc) (conn ?a - loc ?b - loc))
  (:functions (cost))

  (:action move
    :parameters (?a - loc ?b - loc)
    :precondition (and (at ?a) (conn ?a ?b))
    :effect (and (not (at ?a)) (at ?b)
                 (increase (cost) 2))
  )
)
