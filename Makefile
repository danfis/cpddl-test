
ifneq '$(DEBUG)' 'no'
  DEBUG := yes
endif

TOPDIR ?= ..

-include $(TOPDIR)/Makefile.local
include $(TOPDIR)/Makefile.include

CFLAGS += -I./ -I$(TOPDIR)
CFLAGS += $(BORUVKA_CFLAGS)
CFLAGS += $(LP_CFLAGS)
LDFLAGS += -lrt -lm -L$(TOPDIR) -lpddl -pthread
LDFLAGS += $(BORUVKA_LDFLAGS)
LDFLAGS += $(LP_LDFLAGS)
LDFLAGS += $(BLISS_LDFLAGS)
LDFLAGS += $(CLIQUER_LDFLAGS)
LDFLAGS += $(CUDD_LDFLAGS)
LDFLAGS += $(SQLITE_LDFLAGS)

CHECK_REG=cu/cu-check-regressions
CHECK_TS ?=

#TARGETS = test test-pddl test-strips
TARGETS = test

#TESTS  = lisp_file
TESTS  = context
TESTS += pddl
TESTS += lifted_mgroup

TESTS += strips
TESTS += mgroup
TESTS += mutex
#TESTS += sym
#TESTS += disambiguation
#TESTS += h1
#TESTS += irrelevance
#TESTS += invertible
##TESTS += trans_system
#
#TESTS += hff
#TESTS += fdr
#TESTS += famgroup
##TESTS += pot
##TESTS += admissible
##TESTS += tnf
#TESTS += clique
##TESTS += op_mutex_infer
##TESTS += symbolic
##TESTS += datalog

OBJS := $(foreach test,$(TESTS),.objs/$(test).o)
TESTS_C := $(foreach test,$(TESTS),$(test).c)

all: $(TARGETS)

test: test.c test.in.c ../libpddl.a $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)
test.in.c: gen-tests.py $(TESTS_C)
	python3 gen-tests.py $(TESTS_C) >$@

.objs/%.o: %.c %.h %_prob.h
	$(CC) $(CFLAGS) -c -o $@ $<
.objs/%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<
.objs/%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

check: all submodule
	./test $(T) <tasks-base.txt 2>&1 | tee check.log

submodule: pddl-data/test-seq/test/domain.pddl
pddl-data/test-seq/test/domain.pddl:
	git submodule init -- pddl-data
	git submodule update -- pddl-data

check-valgrind: all
	valgrind --leak-check=full --show-reachable=yes --trace-children=yes \
             --error-limit=no \
             ./test $(T) <tasks.txt 2>&1 | tee check.log

check-segfault: all
	valgrind -q --trace-children=yes \
             --error-limit=no \
             ./test $(T) <tasks.txt 2>&1 | tee check.log

check-valgrind-gen-suppressions: all
	valgrind -q --leak-check=full --show-reachable=yes --trace-children=yes \
             --gen-suppressions=all --log-file=out --error-limit=no \
             ./test $(T) <tasks.txt

clean:
	rm -f test.in.c
	rm -f *.o
	rm -f .objs/*.o
	rm -f $(TARGETS)
	rm -f tmp.*
	rm -f reg/tmp.*
	rm -f reg/temp.*

.PHONY: all clean check check-valgrind submodule test-strips-mem
