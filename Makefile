
ifneq '$(DEBUG)' 'no'
  DEBUG := yes
endif

TOPDIR ?= ..

-include $(TOPDIR)/Makefile.config
include $(TOPDIR)/Makefile.include

PYTHON ?= python3

VALGRIND ?= valgrind
VALGRIND_MEMLEAK_OPTS  = --quiet
VALGRIND_MEMLEAK_OPTS += --leak-check=full --show-reachable=yes --show-leak-kinds=all
VALGRIND_MEMLEAK_OPTS += --trace-children=yes --error-limit=no
VALGRIND_MEMLEAK_OPTS += --trace-children-skip-by-arg=find*,diff*,cat*,head*,*validate*
VALGRIND_MEMLEAK_OPTS += --trace-children-skip=*minizinc*,/usr/bin/ls
VALGRIND_MEMLEAK_OPTS += --suppressions=test.supp
#VALGRIND_MEMLEAK_OPTS += --gen-suppressions=yes

VALGRIND_SEGFAULT_OPTS  = --quiet
VALGRIND_SEGFAULT_OPTS += --trace-children=yes --error-limit=no
VALGRIND_SEGFAULT_OPTS += --suppressions=test.supp

T ?= -a -B -p6
T_QUICK ?= -Q -a -p6
T_ALL ?= -A -a -p6
T_FULL ?= -A -a -p6 -m 300

TOOL ?=

CFLAGS += -I./

CHECK_REG=cu/cu-check-regressions
CHECK_TS ?=

TARGETS = test

TESTS  = context
TESTS += pddl
TESTS += lifted_mgroup
TESTS += strips
TESTS += mgroup
TESTS += mutex
TESTS += h1
TESTS += hm
TESTS += disamb
TESTS += sym
TESTS += irrelevance
TESTS += invertible
TESTS += prune_strips
TESTS += fdr
TESTS += hff
TESTS += hm_heur
TESTS += pot
TESTS += lmc
TESTS += flow
TESTS += clique
TESTS += symbolic_search
TESTS += datalog
TESTS += homomorphism
TESTS += endomorphism
TESTS += lifted_heur
TESTS += subprocess
TESTS += lifted_search
TESTS += search
TESTS += gaifman
TESTS += set
TESTS += open_list
TESTS += opts
TESTS += unify
TESTS += num_val
TESTS += fm_num_exp
TESTS += fm_num_eval
TESTS += fm_num_exp_fold
TESTS += pred_func_props
TESTS += pddl_init_state
#TESTS += asnets

OBJS := $(foreach test,$(TESTS),.objs/$(test).o)
TESTS_C := $(foreach test,$(TESTS),src/$(test).c)

C_IN = src/tests_tasks.in.c

all: $(TARGETS)

test: src/test.c src/tasks_tests.c $(C_IN) ../libpddl.a $(OBJS) val/validate
	$(CC) $(CFLAGS) -o $@ $< src/tasks_tests.c $(OBJS) $(LDFLAGS)
src/tests_tasks.in.c: scripts/gen-tests-tasks.py config.toml $(TESTS_C) ../pddl/config.h
	$(PYTHON) scripts/gen-tests-tasks.py config.toml $(TESTS_C) >$@

.objs/%.o: src/%.c src/%.h ../libpddl.a
	$(CC) $(CFLAGS) -c -o $@ $<
.objs/%.o: src/%.c ../libpddl.a
	$(CC) $(CFLAGS) -c -o $@ $<

val/validate: val/Makefile val/src/*.cpp
	$(MAKE) -C val

check: all
	./test -c $(T)
check-quick: all
	./test -c $(T_QUICK)
check-all: all
	./test -c $(T_ALL)
check-tool: ./scripts/test-tool.py config-tool.toml ../bin/pddl-tool
	$(PYTHON) ./scripts/test-tool.py $(TOOL)

check-valgrind: all
	$(VALGRIND) $(VALGRIND_MEMLEAK_OPTS) ./test -c $(T) -vvv -p 1 2>&1 | tee check.log
check-segfault: all
	$(VALGRIND) $(VALGRIND_SEGFAULT_OPTS) ./test -c $(T) -vvv -p 1 2>&1 | tee check.log
check-gdb: all
	gdb --ex 'set follow-fork-mode child' --ex run --args ./test -c $(T)

full-tests: all ./scripts/test-tool.py config-tool.toml ../bin/pddl-tool
	./test -c $(T_FULL) ; $(PYTHON) ./scripts/test-tool.py $(TOOL)

clean:
	rm -f check.log
	rm -f *.o
	rm -f .objs/*.o
	rm -f src/*.in.c
	if [ -x ./test ]; then ./test --clean-reg-and-exit; else find reg/ -name '*.tmp' -exec rm '{}' ';'; fi
	rm -f $(TARGETS)

mrproper: clean
	$(MAKE) -C val clean

clean-reg:
	if [ -x ./test ]; then ./test --clean-reg-and-exit; else find reg/ -name '*.tmp' -exec rm '{}' ';'; fi

.PHONY: all clean mrproper check check-quick check-all \
        check-valgrind check-gdb check-segfault \
        check-tool
