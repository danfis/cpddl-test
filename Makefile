
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

T ?= -a -B -p6

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
TESTS += set
#TESTS += asnets

OBJS := $(foreach test,$(TESTS),.objs/$(test).o)
TESTS_C := $(foreach test,$(TESTS),src/$(test).c)

C_IN = src/tests_tasks.in.c

all: $(TARGETS)

test: src/test.c src/tasks_tests.c $(C_IN) ../libpddl.a $(OBJS) val/validate
	$(CC) $(CFLAGS) -o $@ $< src/tasks_tests.c $(OBJS) $(LDFLAGS)
src/tests_tasks.in.c: scripts/gen-tests-tasks.py config.toml $(TESTS_C) ../pddl/config.h
	python3 scripts/gen-tests-tasks.py config.toml $(TESTS_C) >$@

.objs/%.o: src/%.c src/%.h ../libpddl.a
	$(CC) $(CFLAGS) -c -o $@ $<
.objs/%.o: src/%.c ../libpddl.a
	$(CC) $(CFLAGS) -c -o $@ $<

val/validate: val/Makefile val/src/*.cpp
	$(MAKE) -C val

check: all
	./test $(T)
check-quick: all
	./test -Q $(T)
check-all: all
	./test -A $(T)

check-valgrind: all
	$(VALGRIND) $(VALGRIND_MEMLEAK_OPTS) ./test $(T) -vvv -p 1 2>&1 | tee check.log
check-segfault: all
	$(VALGRIND) $(VALGRIND_SEGFAULT_OPTS) ./test $(T) -vvv -p 1 2>&1 | tee check.log
check-gdb: all
	gdb --ex 'set follow-fork-mode child' --ex run --args ./test $(T)

check-bin: val/validate
	$(SH) bin-tests/run.sh
check-bin-all: val/validate
	$(SH) bin-tests/run.sh --all

clean:
	rm -f check.log
	rm -f *.o
	rm -f .objs/*.o
	rm -f src/*.in.c
	rm -f $(TARGETS)
	find reg/ -name '*.tmp' -exec rm '{}' ';'

mrproper: clean
	$(MAKE) -C val clean

clean-reg:
	find reg/ -name '*.tmp' -exec rm '{}' ';'

.PHONY: all clean mrproper check check-quick check-all \
        check-valgrind check-gdb check-segfault \
        check-bin check-bin-all
