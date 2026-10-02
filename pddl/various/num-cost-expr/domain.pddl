;; Synthetic task where the metric fluent (total-cost) is increased by
;; the expression (+ 1 (g ?b)) over a constant and a static fluent with
;; all initial values non-negative integers.
;; pddlIsMetricExpressibleAsNonNegIntActionCosts() must return true.
(define (domain num-cost-expr)
  (:requirements :strips :typing :numeric-fluents)
  (:types loc)
  (:predicates (at ?l - loc) (conn ?a - loc ?b - loc))
  (:functions (total-cost) (g ?l - loc))

  (:action move
    :parameters (?a - loc ?b - loc)
    :precondition (and (at ?a) (conn ?a ?b))
    :effect (and (not (at ?a)) (at ?b)
                 (increase (total-cost) (+ 1 (g ?b))))
  )
)
