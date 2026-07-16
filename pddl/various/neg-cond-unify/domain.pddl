
(define (domain neg-cond-unify)
  (:requirements :adl :typing)
  (:types obj)
  (:predicates (p ?x - obj) (q ?x - obj) (r ?x - obj))

  ;; The delete effect (not (p ?x)) and the add effect (p ?y) refer to the
  ;; same atom when grounded with ?x = ?y. The add effect must win, so the
  ;; compilation of (not (p ...)) into (NOT-p ...) must not produce an
  ;; operator that makes both (p o) and (NOT-p o) true.
  (:action move
    :parameters (?x ?y - obj)
    :precondition (p ?x)
    :effect (and (not (p ?x)) (p ?y)))

  ;; Cross-scope variant: the conflicting add effect is conditional, so
  ;; whether (NOT-p ?x) becomes true depends on (q ?y) and on ?x = ?y.
  (:action cond-add
    :parameters (?x ?y - obj)
    :precondition (p ?x)
    :effect (and (not (p ?x)) (when (q ?y) (p ?y))))

  ;; As move, but the precondition rules out ?x = ?y, so the compilation
  ;; needs no equality guard: (NOT-p ?x) can be added unconditionally.
  (:action move-diff
    :parameters (?x ?y - obj)
    :precondition (and (p ?x) (not (= ?x ?y)))
    :effect (and (not (p ?x)) (p ?y)))

  ;; As cond-add, but the condition of the conflicting add effect rules
  ;; out ?x = ?y, so no guard is needed either.
  (:action cond-add-diff
    :parameters (?x ?y - obj)
    :precondition (p ?x)
    :effect (and (not (p ?x))
                 (when (and (q ?y) (not (= ?x ?y))) (p ?y))))

  ;; The unconditional add (p ?x) always overrides the conditional delete,
  ;; so the compilation must not emit any (NOT-p ?x) companion for it.
  (:action refresh
    :parameters (?x - obj)
    :precondition (p ?x)
    :effect (and (p ?x) (when (q ?x) (not (p ?x)))))

  ;; Makes q dynamic, so guarding the conflict in cond-add requires
  ;; compiling (not (q ...)) into (NOT-q ...) as well.
  (:action set-q
    :parameters (?x - obj)
    :precondition (r ?x)
    :effect (q ?x))

  ;; Negative precondition on p that triggers the compilation.
  (:action check
    :parameters (?x - obj)
    :precondition (not (p ?x))
    :effect (r ?x))
  )
