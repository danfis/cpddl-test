;; Synthetic task where the metric fluent (total-cost) is increased by a
;; non-negative float constant, so the accumulated value is not provably
;; an integer.
;; pddlIsMetricExpressibleAsNonNegIntActionCosts() must return false.
(define (domain num-cost-float)
  (:requirements :strips :typing :numeric-fluents)
  (:types loc)
  (:predicates (at ?l - loc) (conn ?a - loc ?b - loc))
  (:functions (total-cost))

  (:action move
    :parameters (?a - loc ?b - loc)
    :precondition (and (at ?a) (conn ?a ?b))
    :effect (and (not (at ?a)) (at ?b)
                 (increase (total-cost) 1.5))
  )
)
