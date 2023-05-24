
ifneq '$(DEBUG)' 'no'
  DEBUG := yes
endif

TOPDIR ?= ..

-include $(TOPDIR)/Makefile.config
include $(TOPDIR)/Makefile.include

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
#TESTS += asnets


OBJS := $(foreach test,$(TESTS),.objs/$(test).o)
TESTS_C := $(foreach test,$(TESTS),tests/$(test).c)

C_IN  = test.in.c
C_IN += test.tasks.base.in.c
C_IN += test.tasks.all.in.c
C_IN += tasks_tests.c

all: $(TARGETS)

test: test.c $(C_IN) ../libpddl.a $(OBJS) val/validate
	$(CC) $(CFLAGS) -o $@ $< tasks_tests.c $(OBJS) $(LDFLAGS)
test.in.c: scripts/gen-tests.py $(TESTS_C)
	python3 scripts/gen-tests.py $(TESTS_C) >$@
test.tasks.base.in.c: tasks-base.txt scripts/gen-tasks.py
	python3 scripts/gen-tasks.py base <$< >$@
test.tasks.all.in.c: tasks-base.txt tasks-all.txt scripts/gen-tasks.py
	cat tasks-base.txt tasks-all.txt | python3 scripts/gen-tasks.py all >$@

.objs/%.o: tests/%.c tests/%.h ../libpddl.a
	$(CC) $(CFLAGS) -c -o $@ $<
.objs/%.o: tests/%.c ../libpddl.a
	$(CC) $(CFLAGS) -c -o $@ $<

val/validate: val/Makefile val/src/*.cpp
	$(MAKE) -C val

check: all submodule
	./test $(T) 2>&1 | tee check.log
check-all: all submodule
	./test -a $(T) 2>&1 | tee check.log

submodule: pddl-data/test-seq/test/domain.pddl
pddl-data/test-seq/test/domain.pddl:
	git submodule init -- pddl-data
	git submodule update -- pddl-data

check-valgrind: all clean-reg
	valgrind --leak-check=full --show-reachable=yes --show-leak-kinds=all \
             --trace-children=yes --error-limit=no \
             --child-silent-after-fork=yes \
             --suppressions=test.supp \
             ./test $(T) 2>&1 | tee check.log | bash scripts/filter-valgrind.sh
check-all-valgrind: all clean-reg
	valgrind --leak-check=full --show-reachable=yes --show-leak-kinds=all \
             --trace-children=yes --error-limit=no \
             --child-silent-after-fork=yes \
             --suppressions=test.supp \
             ./test -a $(T) 2>&1 | tee check.log | bash scripts/filter-valgrind.sh

check-segfault: all clean-reg
	valgrind -q --trace-children=yes \
             --error-limit=no \
             --suppressions=test.supp \
             ./test $(T) 2>&1 | tee check.log
check-all-segfault: all clean-reg
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
             ./test $(T)


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
