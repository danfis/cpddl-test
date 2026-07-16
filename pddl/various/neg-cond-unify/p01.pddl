
(define (problem neg-cond-unify-p01)
  (:domain neg-cond-unify)
  (:objects o1 o2 - obj)
  (:init (p o1) (p o2))
  ;; Unsolvable: no action ever increases the number of true (p ...) facts,
  ;; (r o1) requires (p o1) to be false at some point, i.e., at most one
  ;; (p ...) fact true, and the goal requires both again afterwards.
  (:goal (and (r o1) (p o1) (p o2)))
  )
