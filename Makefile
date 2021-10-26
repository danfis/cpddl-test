
ifneq '$(DEBUG)' 'no'
  DEBUG := yes
endif

-include ../Makefile.local
-include ../Makefile.include

CFLAGS += -I./ -I../
CFLAGS += $(BORUVKA_CFLAGS)
CFLAGS += $(LP_CFLAGS)
LDFLAGS += -lrt -lm -L../ -lpddl -pthread
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
TESTS += sym
TESTS += disambiguation
TESTS += h1
TESTS += h2
TESTS += h3
#TESTS += strips_ground
#TESTS += strips_ground_factored
#TESTS += lifted_mgroup_monotonicity
#TESTS += strips_ground_lifted_mgroup
#TESTS += irrelevance
#TESTS += h3
#TESTS += hff
#TESTS += fdr
#TESTS += fdr_fd
#TESTS += sym
#TESTS += famgroup
#TESTS += pot
#TESTS += mg_strips
#TESTS += fdr_app_op
#TESTS += admissible
#TESTS += tnf
TESTS += clique
#TESTS += trans_system
#TESTS += op_mutex_infer
#ifeq '$(USE_CUDD)' 'yes'
#TESTS += symbolic
#endif
#TESTS += invertibility
#TESTS += datalog

OBJS := $(foreach test,$(TESTS),.objs/$(test).o)
TESTS_C := $(foreach test,$(TESTS),$(test).c)

all: $(TARGETS)

main.c: $(OBJS)
	bash gen-main.sh $(TESTS) >$@

test: test.c test.in.c ../libpddl.a $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)
test.in.c: gen-tests.py $(TESTS_C)
	python3 gen-tests.py $(TESTS_C) >$@
test-pddl: test-pddl.c cu/libcu.a ../libpddl.a
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
test-strips: test-strips.c cu/libcu.a ../libpddl.a
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

.objs/%.o: %.c %.h %_prob.h
	$(CC) $(CFLAGS) -c -o $@ $<
.objs/%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<
.objs/%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

check: all submodule
	./test $(CHECK_TS); \
        T=$$?; \
        $(PYTHON2) $(CHECK_REG) reg; \
        exit $$(($$T + $$?))
check-noreg: all submodule
	./test $(CHECK_TS)

check-ci: all submodule
	./test $(CHECK_TS); \
        T=$$?; \
        $(PYTHON2) $(CHECK_REG) --no-progress reg; \
        exit $$(($$T + $$?))

test-strips-mem: test-strips
	@./test-strips-mem.sh pddl-data/test-seq/test/{domain,pfile}.pddl
	@./test-strips-mem.sh pddl-data/ipc-2014/seq-opt/barman/{domain,p433.1}.pddl
	@./test-strips-mem.sh pddl-data/ipc-2014/seq-opt/barman/{domain,p638.1}.pddl
	@./test-strips-mem.sh pddl-data/ipc-2014/seq-opt/cavediving/{domain,testing01}.pddl
	@./test-strips-mem.sh pddl-data/ipc-2014/seq-opt/cavediving/{domain,testing20A_easy}.pddl
	@./test-strips-mem.sh pddl-data/ipc-2014/seq-opt/childsnack/{domain,child-snack_pfile10}.pddl
	@./test-strips-mem.sh pddl-data/ipc-2014/seq-opt/citycar/{domain,p3-3-3-3-2}.pddl
	@./test-strips-mem.sh pddl-data/ipc-2014/seq-opt/floortile/{domain,p03-6-5-2}.pddl
	@./test-strips-mem.sh pddl-data/ipc-2014/seq-opt/ged/{domain,d-8-9}.pddl
	@./test-strips-mem.sh pddl-data/ipc-2014/seq-opt/hiking/{domain,ptesting-2-4-7}.pddl
	@./test-strips-mem.sh pddl-data/ipc-2014/seq-opt/maintenance/{domain,maintenance.1.3.025.100.2-002}.pddl
	@./test-strips-mem.sh pddl-data/ipc-2014/seq-opt/openstacks/{domain_,}p50_3.pddl
	@./test-strips-mem.sh pddl-data/ipc-2014/seq-opt/parking/{domain,p_20_11-04}.pddl
	@./test-strips-mem.sh pddl-data/ipc-2014/seq-opt/tetris/{domain,p01-4}.pddl
	@./test-strips-mem.sh pddl-data/ipc-2014/seq-opt/tetris/{domain,p03-6}.pddl
	@./test-strips-mem.sh pddl-data/ipc-2014/seq-opt/tetris/{domain,p04-10}.pddl
	@./test-strips-mem.sh pddl-data/ipc-2014/seq-opt/transport/{domain,p01}.pddl
	@./test-strips-mem.sh pddl-data/ipc-2014/seq-opt/transport/{domain,p10}.pddl
	@./test-strips-mem.sh pddl-data/ipc-2014/seq-opt/transport/{domain,p20}.pddl
	@./test-strips-mem.sh pddl-data/ipc-2014/seq-opt/visitall/{domain,p-1-5}.pddl
	@./test-strips-mem.sh pddl-data/ipc-2014/seq-opt/visitall/{domain,p-1-18}.pddl
	@./test-strips-mem.sh pddl-data/ipc-2014/seq-opt/visitall/{domain,p-05-10}.pddl
	@./test-strips-mem.sh pddl-data/ipc-2011/seq-opt/scanalyzer/{domain,p01}.pddl
	@./test-strips-mem.sh pddl-data/ipc-2011/seq-opt/scanalyzer/{domain,p10}.pddl
	@./test-strips-mem.sh pddl-data/ipc-2011/seq-opt/scanalyzer/{domain,p20}.pddl

submodule: pddl-data/test-seq/test/domain.pddl
pddl-data/test-seq/test/domain.pddl:
	git submodule init -- pddl-data
	git submodule update -- pddl-data

check-valgrind: all
	valgrind --leak-check=full --show-reachable=yes --trace-children=yes \
             --error-limit=no \
             ./test $(CHECK_TS)

check-segfault: all
	valgrind -q --trace-children=yes \
             --error-limit=no \
             ./test $(CHECK_TS)

check-valgrind-gen-suppressions: all
	valgrind -q --leak-check=full --show-reachable=yes --trace-children=yes \
             --gen-suppressions=all --log-file=out --error-limit=no \
             ./test $(CHECK_TS)

cu: cu/libcu.a
cu/libcu.a:
	$(MAKE) ENABLE_TIMER=yes -C cu/

clean-reg:
	rm -f reg/tmp.*
	rm -f reg/temp.*

clean:
	rm -f main.c
	rm -f *.o
	rm -f .objs/*.o
	rm -f $(TARGETS)
	rm -f tmp.*
	rm -f reg/tmp.*
	rm -f reg/temp.*
	$(MAKE) -C cu clean

.PHONY: all clean clean-reg check check-valgrind cu submodule test-strips-mem
