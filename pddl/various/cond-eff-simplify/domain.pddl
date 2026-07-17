;; Synthetic domain exercising pddlActionSimplifyCondEffs(): the single
;; action has one conditional effect for each simplification case.
(define (domain cond-eff-simplify)
  (:requirements :adl :typing)
  (:types item)
  (:predicates
     (pre-a ?x - item)
     (pre-b ?x - item)
     (extra ?x - item)
     (done ?x - item)
     (eff-entailed-atom ?x - item)
     (eff-fully-entailed ?x - item)
     (eff-conflict ?x - item)
     (eff-untouched ?x - item))

  (:action process
    :parameters (?x - item)
    :precondition (and (pre-a ?x) (pre-b ?x))
    :effect (and
      (done ?x)
      ;; (pre-a ?x) is entailed by the precondition and is dropped from the
      ;; condition, leaving (when (extra ?x) (eff-entailed-atom ?x)).
      (when (and (pre-a ?x) (extra ?x)) (eff-entailed-atom ?x))
      ;; The whole condition is entailed by the precondition, so this becomes
      ;; an unconditional effect.
      (when (and (pre-a ?x) (pre-b ?x)) (eff-fully-entailed ?x))
      ;; The condition conflicts with the precondition, so the whole
      ;; conditional effect is removed.
      (when (not (pre-a ?x)) (eff-conflict ?x))
      ;; Unrelated to the precondition, so it is left unchanged.
      (when (extra ?x) (eff-untouched ?x))))
  )
