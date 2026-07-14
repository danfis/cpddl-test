#!/usr/bin/env python3
"""Ad-hoc semantic + structural comparison of conditional-effect
compilations: cpddl's --ce cocoa vs the reference implementation
(https://gitlab.com/EdmondDantes/cocoa2.0).

Inputs (per task): three strips-as-py exports produced by bin/pddl-tool:
  orig.py  -- the original grounded task, conditional effects preserved
  ours.py  -- the task compiled with pddlStripsCompileAwayCondEffCocoa()
  ref.py   -- the reference's compiled PDDL, grounded by cpddl

Semantic test: seeded random walks on the original task. Every step
applies one original operator (a) directly, using conditional-effect
semantics (all conditions evaluated on the pre-state, add-wins), and
(b) through the corresponding compiled operator chain in each
compilation. After each step the compiled states are projected onto the
original atoms and compared with the ground-truth successor.

Structural report: operator/fact counts, auxiliary operators, twin atoms.
"""
import random
import sys


def load(path):
    with open(path) as f:
        src = f.read()
    task = eval(src, {'set': set, 'False': False, 'True': True})
    for op in task['op']:
        op.setdefault('cond_eff', [])
    return task


def orig_key(name):
    # '(white r2 c1)' -> 'white_r2_c1'
    return name[1:-1].replace(' ', '_')


def ref_key(name):
    # reference atoms are 0-ary predicates: '(white_r2_c1)' -> 'white_r2_c1';
    # atoms of 0-ary original predicates carry a trailing '_': '(a_)' -> 'a'
    key = name[1:-1]
    if key.endswith('_'):
        key = key[:-1]
    return key


OUR_AUX_FACT = ('cocoa-', 'NOT-')
REF_AUX_FACT = ('pause_condeff', 'evaluation_done_', 'evaluating_',
                'copy_z_', 'NOT-', 'not-')


def is_aux_fact(key, prefixes):
    return key.startswith(prefixes)


def atom_map(task, key_fn, aux_prefixes):
    """fact id -> original-atom key for non-auxiliary facts"""
    m = {}
    for i, name in enumerate(task['fact']):
        key = key_fn(name)
        if not is_aux_fact(key, aux_prefixes):
            m[i] = key
    return m


def proj(state, amap):
    return frozenset(amap[i] for i in state if i in amap)


def successor(op, state):
    """Ground-truth conditional-effect semantics: all conditions evaluated
    on STATE, union of effects, add wins on conflicts."""
    adds = set(op['add'])
    dels = set(op['del'])
    for ce in op['cond_eff']:
        if ce['pre'] <= state:
            adds |= ce['add']
            dels |= ce['del']
    return (state - dels) | adds


def apply_op(op, state):
    return (state - op['del']) | op['add']


class Compiled:
    """Generic driver executing the compiled chain of one original op."""

    def __init__(self, task, key_fn, aux_prefixes):
        self.task = task
        self.amap = atom_map(task, key_fn, aux_prefixes)
        self.by_name = {}
        for op in task['op']:
            self.by_name.setdefault(op['name'], []).append(op)
        self.state = set(task['init'])

    def find_applicable(self, names):
        for n in names:
            for op in self.by_name.get(n, []):
                if op['pre'] <= self.state:
                    return op
        return None

    def chain_ops(self, orig_name):
        raise NotImplementedError

    def start_names(self, orig_name):
        raise NotImplementedError

    def done(self):
        raise NotImplementedError

    def sim(self, orig_name):
        """Simulate one original operator; returns error string or None."""
        op = self.find_applicable(self.start_names(orig_name))
        if op is None:
            return 'no applicable start operator'
        self.state = apply_op(op, self.state)
        chain = self.chain_ops(orig_name)
        for _ in range(10000):
            if self.done():
                return None
            nxt = None
            for cop in chain:
                if cop['pre'] <= self.state:
                    nxt = cop
                    break
            if nxt is None:
                return 'chain stuck (no applicable chain operator)'
            self.state = apply_op(nxt, self.state)
        return 'chain did not terminate'


class OursCompiled(Compiled):
    def __init__(self, task):
        super().__init__(task, orig_key, OUR_AUX_FACT)
        self.free = [i for i, n in enumerate(task['fact'])
                     if n == '(cocoa-free)']
        self.noset = [i for i, n in enumerate(task['fact'])
                      if n == '(cocoa-noset)']
        self._chain_cache = {}

    def start_names(self, orig_name):
        return [orig_name]

    def chain_ops(self, orig_name):
        if orig_name not in self._chain_cache:
            prefix = orig_name + ' cocoa-'
            ops = [op for op in self.task['op']
                   if op['name'].startswith(prefix)]
            ops.sort(key=lambda o: o['name'])
            self._chain_cache[orig_name] = ops
        return self._chain_cache[orig_name]

    def done(self):
        for i in self.free + self.noset:
            if i not in self.state:
                return False
        return True


class RefCompiled(Compiled):
    def __init__(self, task):
        super().__init__(task, ref_key, REF_AUX_FACT)
        self.pause = [i for i, n in enumerate(task['fact'])
                      if n == '(pause_condeff)']
        orig_names = set()
        for op in task['op']:
            n = op['name']
            if not n.startswith(('start-sequence___', 'eval_pos_',
                                 'eval_neg_', 'close_eval_', 'setup_')):
                orig_names.add(n)
        self.aux_ops = sorted(
            (op for op in task['op']
             if op['name'] not in orig_names
             and not op['name'].startswith('start-sequence___')),
            key=lambda o: o['name'])

    def start_names(self, orig_name):
        # 'move agent-1 x y' -> 'move#agent-1#x#y'; 0-ary 'cyc' -> 'cyc#'
        n = orig_name.replace(' ', '#')
        variants = [n, n + '#']
        return variants + ['start-sequence___' + v for v in variants]

    def chain_ops(self, orig_name):
        # setup_* operators carry no action name, so the chain has to be
        # selected by applicability among all auxiliary operators; the
        # pause/evaluating flags make only this chain applicable
        return self.aux_ops

    def done(self):
        return all(i not in self.state for i in self.pause)


def structural(name, orig, ours, ref):
    o_aux = sum(1 for op in ours['op'] if ' cocoa-' in op['name'])
    o_twin = sum(1 for f in ours['fact'] if f.startswith('(cocoa-twin'))
    r_aux = sum(1 for op in ref['op']
                if op['name'].startswith(('eval_pos_', 'eval_neg_',
                                          'close_eval_', 'setup_',
                                          'start-sequence___')))
    r_twin = sum(1 for f in ref['fact'] if f.startswith('(copy_z_'))
    ce_ops = sum(1 for op in orig['op'] if op['cond_eff'])
    print(f"  [struct] orig: ops={len(orig['op'])} (cond-eff={ce_ops}) "
          f"facts={len(orig['fact'])}")
    print(f"  [struct] ours: ops={len(ours['op'])} (aux={o_aux}) "
          f"facts={len(ours['fact'])} twin-facts={o_twin}")
    print(f"  [struct] ref : ops={len(ref['op'])} (aux={r_aux}) "
          f"facts={len(ref['fact'])} twin-facts={r_twin}")


def run_task(name, orig_path, ours_path, ref_path,
             walks=20, steps=20, seed=42):
    orig = load(orig_path)
    ours = load(ours_path)
    ref = load(ref_path)
    print(f"=== {name}")
    structural(name, orig, ours, ref)

    orig_amap = atom_map(orig, orig_key, ('NOT-',))
    rng = random.Random(seed)
    ours_fail = ref_fail = 0
    ours_err = ref_err = None
    napplied = 0

    for walk in range(walks):
        state = set(orig['init'])
        ours_c = OursCompiled(ours)
        ref_c = RefCompiled(ref)

        # common atoms; the reference pipeline (FD grounder) may prune
        # atoms it proves constant, so compare on the intersection
        common = proj(set(range(len(orig['fact']))), orig_amap) \
            & proj(set(range(len(ref_c.task['fact']))), ref_c.amap)

        def check(step, opname):
            nonlocal ours_fail, ref_fail, ours_err, ref_err
            truth = proj(state, orig_amap)
            p_ours = proj(ours_c.state, ours_c.amap)
            if truth != p_ours and ours_fail == 0:
                d = truth ^ p_ours
                ours_err = (f"walk {walk} step {step} op '{opname}': "
                            f"differs on {sorted(d)[:6]}")
            if truth != p_ours:
                ours_fail += 1
            t_common = frozenset(k for k in truth if k in common)
            p_ref = frozenset(k for k in proj(ref_c.state, ref_c.amap)
                              if k in common)
            if t_common != p_ref and ref_fail == 0:
                d = t_common ^ p_ref
                ref_err = (f"walk {walk} step {step} op '{opname}': "
                           f"differs on {sorted(d)[:6]}")
            if t_common != p_ref:
                ref_fail += 1

        check(-1, '<init>')
        for step in range(steps):
            app = [op for op in orig['op'] if op['pre'] <= state]
            if not app:
                break
            op = rng.choice(app)
            state = successor(op, state)
            e = ours_c.sim(op['name'])
            if e is not None and ours_err is None:
                ours_err = f"walk {walk} step {step} op '{op['name']}': {e}"
                ours_fail += 1
            e = ref_c.sim(op['name'])
            if e is not None and ref_err is None:
                ref_err = f"walk {walk} step {step} op '{op['name']}': {e}"
                ref_fail += 1
            napplied += 1
            check(step, op['name'])
            if ours_fail and ref_fail:
                break
        if ours_fail and ref_fail:
            break

    ok = True
    print(f"  [sim] {napplied} operator applications simulated")
    if ours_fail:
        print(f"  [sim] OURS DIVERGED ({ours_fail}x): {ours_err}")
        ok = False
    else:
        print("  [sim] ours: all states match ground truth")
    if ref_fail:
        print(f"  [sim] REF DIVERGED ({ref_fail}x): {ref_err}")
    else:
        print("  [sim] ref : all states match ground truth")
    return ok, ref_fail == 0


def main():
    tasks = sys.argv[1:]
    ours_all_ok = True
    ref_results = []
    for t in tasks:
        name, orig_p, ours_p, ref_p = t.split(',')
        ok, ref_ok = run_task(name, orig_p, ours_p, ref_p)
        ours_all_ok &= ok
        ref_results.append((name, ref_ok))
    print()
    print("SUMMARY ours:", "PASS" if ours_all_ok else "FAIL")
    print("SUMMARY ref :", " ".join(f"{n}={'ok' if r else 'DIVERGED'}"
                                    for n, r in ref_results))
    sys.exit(0 if ours_all_ok else 1)


if __name__ == '__main__':
    main()
