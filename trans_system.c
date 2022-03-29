#include <assert.h>
#include "test.h"
#include "context.h"

static int selectRandomApplicableOp(pddl_rand_t *rnd,
                                    const pddl_strips_t *strips,
                                    const pddl_iset_t *state)
{
    PDDL_ISET(app);
    for (int op_id = 0; op_id < strips->op.op_size; ++op_id){
        const pddl_strips_op_t *op = strips->op.op[op_id];
        if (pddlISetIsSubset(&op->pre, state))
            pddlISetAdd(&app, op_id);
    }

    int op = -1;
    if (pddlISetSize(&app) > 0){
        int idx = pddlRand(rnd, 0, pddlISetSize(&app));
        op = pddlISetGet(&app, idx);
    }
    pddlISetFree(&app);
    return op;
}

static void checkTransition(const pddl_trans_systems_t *tss,
                            int ts_id,
                            int from,
                            int to,
                            int label)
{
    const pddl_trans_system_t *ts = tss->ts[ts_id];
    for (int ltri = 0; ltri < ts->trans.trans_size; ++ltri){
        const pddl_label_set_t *lbl = ts->trans.trans[ltri].label;
        const pddl_transitions_t *trs = &ts->trans.trans[ltri].trans;
        for (int tri = 0; tri < trs->trans_size; ++tri){
            if (from == trs->trans[tri].from && to == trs->trans[tri].to){
                assert(pddlISetIn(label, &lbl->label));
                return;
            }
        }
    }
    //fprintf(stderr, "E %d %d->%d %d\n", ts_id, from, to, label);
    fprintf(stderr, "Error: The transition not found\n");
    assert(0);
}

static void testRandomWalk(pddl_rand_t *rnd,
                           const pddl_strips_t *strips,
                           pddl_lm_cut_t *lmc,
                           const pddl_trans_systems_t *tss,
                           int ts_id,
                           int length)
{
    //fprintf(stderr, "Random Walk for %d\n", ts_id);
    PDDL_ISET(state);
    pddlISetUnion(&state, &strips->init);

    int state_id = pddlTransSystemsStripsState(tss, ts_id, &state);
    assert(state_id == tss->ts[ts_id]->init_state);

    //fprintf(stderr, "init-id: %d\n", state_id);
    int op_id = selectRandomApplicableOp(rnd, strips, &state);
    assert(!pddlISetIn(op_id, &tss->dead_labels));
    for (int i = 0; i < length && op_id >= 0; ++i){
        const pddl_strips_op_t *op = strips->op.op[op_id];
        pddlISetMinus(&state, &op->del_eff);
        pddlISetUnion(&state, &op->add_eff);
        int next_id = pddlTransSystemsStripsState(tss, ts_id, &state);
        //fprintf(stderr, "step %d: %d->%d [%d:(%s)]\n", i, state_id, next_id,
        //                op_id, op->name);
        if (next_id < 0){
            int heur = pddlLMCutStrips(lmc, &state, NULL, NULL);
            if (heur != PDDL_COST_DEAD_END){
                int is_dead = pddlH2IsDeadEnd(strips, &state);
                if (!is_dead){
                    fprintf(stderr, "Unverified dead-end:");
                    int fact;
                    PDDL_ISET_FOR_EACH(&state, fact)
                        fprintf(stderr, " %d", fact);
                    fprintf(stderr, "\n");
                }
            }

            pddlISetFree(&state);
            return;
        }
        checkTransition(tss, ts_id, state_id, next_id, op_id);

        op_id = selectRandomApplicableOp(rnd, strips, &state);
        assert(!pddlISetIn(op_id, &tss->dead_labels));
        state_id = next_id;
    }
    pddlISetFree(&state);
}

static void testAbstrMap(pddl_trans_systems_t *tss, int ts_id)
{
    const pddl_trans_system_t *ts = tss->ts[ts_id];
    pddl_trans_system_abstr_map_t map;
    pddlTransSystemAbstrMapInit(&map, ts->num_states);
    assert(ts->num_states == map.num_states);
    assert(map.map_num_states < 0);

    int *dist = PDDL_ALLOC_ARR(int, ts->num_states);
    pddl_trans_system_graph_t graph;

    pddlTransSystemGraphInit(&graph, ts);
    pddlTransSystemGraphFwDist(&graph, dist);
    for (int s = 0; s < ts->num_states; ++s){
        if (dist[s] < 0)
            pddlTransSystemAbstrMapPruneState(&map, s);
    }
    pddlTransSystemGraphBwDist(&graph, dist);
    for (int s = 0; s < ts->num_states; ++s){
        if (dist[s] < 0)
            pddlTransSystemAbstrMapPruneState(&map, s);
    }

    pddl_set_iset_t comp;
    pddlSetISetInit(&comp);
    pddlTransSystemGraphFwSCC(&graph, &comp);


    for (int ci = 0; ci < pddlSetISetSize(&comp); ++ci){
        const pddl_iset_t *c = pddlSetISetGet(&comp, ci);
        if (pddlISetSize(c) > 1)
            pddlTransSystemAbstrMapCondense(&map, c);
    }
    pddlSetISetFree(&comp);

    /*
    fprintf(stdout, "Before[%d]:", map.map_num_states);
    for (int s = 0; s < map.num_states; ++s){
        fprintf(stdout, " %d", map.map[s]);
    }
    fprintf(stdout, "\n");
    */

    pddlTransSystemAbstrMapFinalize(&map);
    fprintf(stdout, "    Map[%d]:", map.map_num_states);
    for (int s = 0; s < map.num_states; ++s){
        fprintf(stdout, " %d", map.map[s]);
    }
    fprintf(stdout, "\n");
    fflush(stdout);

    if (!map.is_identity){
        fprintf(stdout, "    Abstracted ");
        int abstr_tsi = pddlTransSystemsCloneTransSystem(tss, ts_id);
        pddlTransSystemsAbstract(tss, abstr_tsi, &map);
        pddlTransSystemPrintDebug2(tss, abstr_tsi, stdout);

        if (tss->ts[abstr_tsi]->num_states > 1){
            const pddl_trans_system_t *ats = tss->ts[abstr_tsi];
            pddl_trans_system_graph_t graph;
            pddlTransSystemGraphInit(&graph, ats);

            pddl_iset_t *reach = PDDL_CALLOC_ARR(pddl_iset_t, graph.num_states);
            pddlTransSystemGraphFwReachability(&graph, reach, 0);
            fprintf(stdout, "    Reachable:");
            for (int i = 0; i < graph.num_states; ++i){
                fprintf(stdout, " [%d]<-[", i);
                pddlISetPrintCompressed(reach + i, stdout);
                fprintf(stdout, "]");
            }
            fprintf(stdout, "\n");
            for (int i = 0; i < graph.num_states; ++i)
                pddlISetFree(reach + i);
            PDDL_FREE(reach);
            pddlTransSystemGraphFree(&graph);
        }
    }

    pddlTransSystemAbstrMapFree(&map);
    pddlTransSystemGraphFree(&graph);
    PDDL_FREE(dist);
}

static void printTS(const pddl_trans_systems_t *tss,
                    int tsi,
                    const pddl_mg_strips_t *mg_strips)
{
    const pddl_trans_system_t *ts = tss->ts[tsi];
    if (tsi != 0)
        fprintf(stdout, "\n");

    pddlTransSystemPrintDebug2(tss, tsi, stdout);

    int *dist = PDDL_ALLOC_ARR(int, ts->num_states);
    pddl_trans_system_graph_t graph;
    pddlTransSystemGraphInit(&graph, ts);
    pddlTransSystemGraphFwDist(&graph, dist);
    fprintf(stdout, "    fw-dist:");
    for (int i = 0; i < ts->num_states; ++i)
        fprintf(stdout, " %d", dist[i]);
    fprintf(stdout, "\n");

    pddlTransSystemGraphBwDist(&graph, dist);
    fprintf(stdout, "    bw-dist:");
    for (int i = 0; i < ts->num_states; ++i)
        fprintf(stdout, " %d", dist[i]);
    fprintf(stdout, "\n");

    pddl_set_iset_t comp;
    pddlSetISetInit(&comp);
    pddlTransSystemGraphFwSCC(&graph, &comp);
    fprintf(stdout, "    fw-scc:");
    for (int ci = 0; ci < pddlSetISetSize(&comp); ++ci){
        const pddl_iset_t *c = pddlSetISetGet(&comp, ci);
        fprintf(stdout, " [");
        pddlISetPrintCompressed(c, stdout);
        fprintf(stdout, "]");
    }
    fprintf(stdout, "\n");
    pddlSetISetFree(&comp);

    pddlSetISetInit(&comp);
    pddlTransSystemGraphBwSCC(&graph, &comp);
    fprintf(stdout, "    bw-scc:");
    for (int ci = 0; ci < pddlSetISetSize(&comp); ++ci){
        const pddl_iset_t *c = pddlSetISetGet(&comp, ci);
        fprintf(stdout, " [");
        pddlISetPrintCompressed(c, stdout);
        fprintf(stdout, "]");
    }
    fprintf(stdout, "\n");
    pddlSetISetFree(&comp);

    pddlTransSystemGraphFree(&graph);
    PDDL_FREE(dist);
}

TEST(trans_system, mg_strips)
{
    pddlTransSystemsInit(&C.trans_systems, &C.mg_strips, &C.mutex);
    assert(C.trans_systems.ts_size > 0);
    C.trans_systems_set = 1;

    pddlTransSystemsCollectDeadLabelsFromAll(&C.trans_systems);
    pddlTransSystemsRemoveDeadLabelsFromAll(&C.trans_systems);

}

TEST(trans_system_init, trans_system)
{
    int ts_size = C.trans_systems.ts_size;
    for (int tsi = 0; tsi < ts_size; ++tsi){
        const pddl_trans_system_t *ts = C.trans_systems.ts[tsi];
        assert(ts->num_states > 1);
    }

    for (int tsi = 0; tsi < ts_size; ++tsi){
        const pddl_trans_system_t *ts = C.trans_systems.ts[tsi];
        assert(ts->num_states > 1);
        printTS(&C.trans_systems, tsi, &C.mg_strips);
        testAbstrMap(&C.trans_systems, tsi);
    }

    pddl_lm_cut_t lmc;
    pddlLMCutInitStrips(&lmc, &C.mg_strips.strips, 0, 0);

    pddl_rand_t rnd;
    //pddlRandInitSeed(&rnd, 123);
    pddlRandInit(&rnd);
    ts_size = PDDL_MIN(C.trans_systems.ts_size, 30);
    for (int tsi = 0; tsi < ts_size; ++tsi){
        for (int i = 0; i < 30; ++i)
            testRandomWalk(&rnd, &C.mg_strips.strips, &lmc, &C.trans_systems, tsi, 100);
    }

    pddlLMCutFree(&lmc);
}

TEST(trans_system_merge, trans_system)
{
    int merge_tsi_first = C.trans_systems.ts_size;
    int max_ts_size = PDDL_MIN(C.trans_systems.ts_size, 3);
    for (int i1 = 0; i1 < max_ts_size; ++i1){
        for (int i2 = i1 + 1; i2 < max_ts_size; ++i2){
            pddlTransSystemsMerge(&C.trans_systems, i1, i2);
        }
    }

    pddlTransSystemsCollectDeadLabelsFromAll(&C.trans_systems);
    pddlTransSystemsRemoveDeadLabelsFromAll(&C.trans_systems);

    int ts_size = C.trans_systems.ts_size;
    for (int tsi = merge_tsi_first; tsi < ts_size; ++tsi){
        const pddl_trans_system_t *ts = C.trans_systems.ts[tsi];
        assert(ts->num_states > 1);
        printTS(&C.trans_systems, tsi, &C.mg_strips);
        testAbstrMap(&C.trans_systems, tsi);
    }

    pddl_lm_cut_t lmc;
    pddlLMCutInitStrips(&lmc, &C.mg_strips.strips, 0, 0);

    pddl_rand_t rnd;
    //pddlRandInitSeed(&rnd, 123);
    pddlRandInit(&rnd);
    for (int tsi = merge_tsi_first; tsi < C.trans_systems.ts_size; ++tsi){
        for (int i = 0; i < 30; ++i)
            testRandomWalk(&rnd, &C.mg_strips.strips, &lmc, &C.trans_systems, tsi, 100);
    }

    pddlLMCutFree(&lmc);
}
