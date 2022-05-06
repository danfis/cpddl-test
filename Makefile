
ifneq '$(DEBUG)' 'no'
  DEBUG := yes
endif

TOPDIR ?= ..

-include $(TOPDIR)/Makefile.local
include $(TOPDIR)/Makefile.include

CFLAGS += -I./ -I$(TOPDIR)
LDFLAGS += -lrt -lm -L$(TOPDIR) -lpddl -pthread
LDFLAGS += $(CPOPTIMIZER_LDFLAGS)
LDFLAGS += $(LP_LDFLAGS)
LDFLAGS += $(BLISS_LDFLAGS)
LDFLAGS += $(CLIQUER_LDFLAGS)
LDFLAGS += $(CUDD_LDFLAGS)

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


OBJS := $(foreach test,$(TESTS),.objs/$(test).o)
TESTS_C := $(foreach test,$(TESTS),$(test).c)

C_IN  = test.in.c
C_IN += test.tasks.base.in.c
C_IN += test.tasks.all.in.c

all: $(TARGETS)

test: test.c $(C_IN) ../libpddl.a $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)
test.in.c: gen-tests.py $(TESTS_C)
	python3 gen-tests.py $(TESTS_C) >$@
test.tasks.base.in.c: tasks-base.txt gen-tasks.py
	python3 gen-tasks.py base <$< >$@
test.tasks.all.in.c: tasks-base.txt tasks-noce.txt gen-tasks.py
	cat tasks-base.txt tasks-noce.txt | python3 gen-tasks.py all >$@

.objs/%.o: %.c %.h ../libpddl.a
	$(CC) $(CFLAGS) -c -o $@ $<
.objs/%.o: %.c ../libpddl.a
	$(CC) $(CFLAGS) -c -o $@ $<

check: all submodule
	./test $(T) 2>&1 | tee check.log
check-all: all submodule
	./test -a $(T) 2>&1 | tee check.log

submodule: pddl-data/test-seq/test/domain.pddl
pddl-data/test-seq/test/domain.pddl:
	git submodule init -- pddl-data
	git submodule update -- pddl-data

check-valgrind: all
	valgrind --leak-check=full --show-reachable=yes --show-leak-kinds=all \
             --trace-children=yes --error-limit=no \
             --child-silent-after-fork=yes \
             --suppressions=test.supp \
             ./test $(T) 2>&1 | tee check.log | bash filter-valgrind.sh
check-all-valgrind: all
	valgrind --leak-check=full --show-reachable=yes --show-leak-kinds=all \
             --trace-children=yes --error-limit=no \
             --child-silent-after-fork=yes \
             --suppressions=test.supp \
             ./test -a $(T) 2>&1 | tee check.log | bash filter-valgrind.sh

check-segfault: all
	valgrind -q --trace-children=yes \
             --error-limit=no \
             --suppressions=test.supp \
             ./test $(T) 2>&1 | tee check.log
check-all-segfault: all
	valgrind -q --trace-children=yes \
             --error-limit=no \
             --suppressions=test.supp \
             ./test -a $(T) 2>&1 | tee check.log

check-gdb: all
	gdb --ex 'set follow-fork-mode child' --ex run --args ./test $(T)
check-all-gdb: all
	gdb --ex 'set follow-fork-mode child' --ex run --args ./test -a $(T)

check-valgrind-gen-suppressions: all
	valgrind -q --leak-check=full --show-reachable=yes --trace-children=yes \
             --gen-suppressions=all --log-file=supp.out --error-limit=no \
             --suppressions=test.supp \
             ./test $(T) <tasks.txt

clean:
	rm -f test.in.c
	rm -f *.o
	rm -f .objs/*.o
	rm -f *.in.c
	rm -f $(TARGETS)
	rm -f tmp.*
	rm -f reg/tmp.*
	rm -f reg/temp.*

.PHONY: all clean check check-valgrind submodule test-strips-mem
