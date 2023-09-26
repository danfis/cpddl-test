
ifneq '$(DEBUG)' 'no'
  DEBUG := yes
endif

TOPDIR ?= ..

-include $(TOPDIR)/Makefile.config
include $(TOPDIR)/Makefile.include

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

T ?= -a -B

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
TESTS += sym
TESTS += irrelevance
TESTS += invertible
TESTS += fdr
TESTS += hff
TESTS += pot
TESTS += lmc
TESTS += flow
TESTS += clique
TESTS += symbolic
TESTS += datalog
TESTS += homomorphism
TESTS += endomorphism
TESTS += lifted_heur
TESTS += subprocess
TESTS += lifted_search
TESTS += search
TESTS += gaifman
#TESTS += asnets


OBJS := $(foreach test,$(TESTS),.objs/$(test).o)
TESTS_C := $(foreach test,$(TESTS),tests/$(test).c)

C_IN  = test.in.c
C_IN += tasks.in.c

all: $(TARGETS)

test: test.c tasks_tests.c $(C_IN) ../libpddl.a $(OBJS) val/validate
	$(CC) $(CFLAGS) -o $@ $< tasks_tests.c $(OBJS) $(LDFLAGS)
test.in.c: scripts/gen-tests.py $(TESTS_C)
	python3 scripts/gen-tests.py $(TESTS_C) >$@
tasks.in.c: tasks-disable.txt tasks-base.txt tasks-all.txt scripts/gen-tasks.py
	python3 scripts/gen-tasks.py tasks-disable.txt tasks-base.txt tasks-all.txt >$@

.objs/%.o: tests/%.c tests/%.h ../libpddl.a
	$(CC) $(CFLAGS) -c -o $@ $<
.objs/%.o: tests/%.c ../libpddl.a
	$(CC) $(CFLAGS) -c -o $@ $<

val/validate: val/Makefile val/src/*.cpp
	$(MAKE) -C val

check: all submodule
	./test $(T) 2>&1 | tee check.log
check-all: all submodule
	./test -A $(T) 2>&1 | tee check.log

submodule: pddl-data/test-seq/test/domain.pddl
pddl-data/test-seq/test/domain.pddl:
	git submodule init -- pddl-data
	git submodule update -- pddl-data

check-valgrind: all clean-reg
	$(VALGRIND) $(VALGRIND_MEMLEAK_OPTS) ./test $(T) -vvv -p 1 2>&1 | tee check.log
check-segfault: all clean-reg
	$(VALGRIND) $(VALGRIND_SEGFAULT_OPTS) ./test $(T) -vvv -p 1 2>&1 | tee check.log
check-gdb: all
	gdb --ex 'set follow-fork-mode child' --ex run --args ./test $(T)

check-bin: val/validate
	$(SH) bin-tests/run.sh
check-bin-all: val/validate
	$(SH) bin-tests/run.sh --all

clean:
	$(MAKE) -C val clean
	rm -f check.log
	rm -f *.o
	rm -f .objs/*.o
	rm -f *.in.c
	rm -f $(TARGETS)
	find reg/ -name '*.tmp' -exec rm '{}' ';'

clean-reg:
	find reg/ -name '*.tmp' -exec rm '{}' ';'

.PHONY: all clean check check-valgrind submodule test-strips-mem \
        check-bin check-bin-search-opt check-bin-search-sat
