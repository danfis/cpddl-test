#include "pddl/pddl.h"
#include "test.h"
#include "context.h"
#include <assert.h>

static void genRandGraph(pddl_rand_t *rnd,
                         pddl_graph_simple_t *g,
                         int max_nodes,
                         float p)
{
    int nodes = pddlRand(rnd, 3, max_nodes);
    pddlGraphSimpleInit(g, nodes);
    for (int i = 0; i < nodes; ++i){
        for (int j = i + 1; j < nodes; ++j){
            if (pddlRand(rnd, 0, 1.) >= p){
                pddlGraphSimpleAddEdge(g, i, j);
            }
        }
    }
}

static void addClique(const pddl_iset_t *clique, void *ud)
{
    pddl_set_iset_t *sset = ud;
    assert(pddlSetISetFind(sset, clique) == -1);
    pddlSetISetAdd(sset, clique);
}

TEST_ONCE(clique_rand)
{
    pddl_rand_t rnd;
    pddlRandInitAuto(&rnd);

    for (int _i = 0; _i < 100; ++_i){
        pddl_graph_simple_t g;
        genRandGraph(&rnd, &g, 30, .5);

        pddl_set_iset_t sset;
        pddlSetISetInit(&sset);
        pddlCliqueFindMaximal(&g, addClique, &sset);
        int size = pddlSetISetSize(&sset);
#ifdef PDDL_CLIQUER
        pddl_set_iset_t sset2;
        pddlSetISetInit(&sset2);
        pddlCliqueFindMaximalCliquer(&g, addClique, &sset2);
        int size2 = pddlSetISetSize(&sset2);
        assert(size == size2);
        for (int i = 0; i < size && i < size2; ++i){
            const pddl_iset_t *clique = pddlSetISetGet(&sset, i);
            const pddl_iset_t *clique2 = pddlSetISetGet(&sset2, i);
            assert(pddlSetISetFind(&sset2, clique) >= 0);
            assert(pddlSetISetFind(&sset, clique2) >= 0);
        }

        pddlSetISetFree(&sset2);
#endif
        //printf(":: %d %d\n", _i, size);
        for (int i = 0; i < size; ++i){
            const pddl_iset_t *clique = pddlSetISetGet(&sset, i);

            // Check that clique is indeed a clique
            int csize = pddlISetSize(clique);
            for (int ci = 0; ci < csize; ++ci){
                int n1 = pddlISetGet(clique, ci);
                for (int cj = ci + 1; cj < csize; ++cj){
                    int n2 = pddlISetGet(clique, cj);
                    assert(pddlISetIn(n2, &g.node[n1]));
                }
            }
        }

        // Check that there are not any subsets
        for (int i = 0; i < size; ++i){
            const pddl_iset_t *clique = pddlSetISetGet(&sset, i);
            for (int j = i + 1; j < size; ++j){
                const pddl_iset_t *clique2 = pddlSetISetGet(&sset, j);
                assert(!pddlISetIsSubset(clique, clique2));
                assert(!pddlISetIsSubset(clique2, clique));
            }
        }
        pddlSetISetFree(&sset);

        pddlGraphSimpleFree(&g);
    }
}

static void addBiclique(const pddl_iset_t *left,
                        const pddl_iset_t *right,
                        void *ud)
{
    pddl_set_iset_t *sset = ud;
    assert(pddlISetIsDisjunct(left, right));
    PDDL_ISET(biclique);
    //if (pddlISetCmp(left, right) < 0){
        pddlISetUnion(&biclique, left);
        PDDL_ISET_FOR_EACH(right, n)
            pddlISetAdd(&biclique, 1000 + n);
        /*
    }else{
        pddlISetUnion(&biclique, right);
        int n;
        PDDL_ISET_FOR_EACH(left, n)
            pddlISetAdd(&biclique, 1000 + n);
    }
    */
    //assertEquals(pddlSetISetFind(sset, &biclique), -1);
    pddlSetISetAdd(sset, &biclique);
    pddlISetFree(&biclique);
}

TEST_ONCE(clique_biclique_rand)
{
    pddl_rand_t rnd;
    pddlRandInitAuto(&rnd);
    //pddlRandInitSeed(&rnd, 0);

    for (int _i = 0; _i < 100; ++_i){
        pddl_graph_simple_t g;
        genRandGraph(&rnd, &g, 20, .5);

        pddl_set_iset_t sset;
        pddlSetISetInit(&sset);
        pddlCliqueFindMaximalBicliques(&g, addBiclique, &sset);
        int size = pddlSetISetSize(&sset);

        pddl_set_iset_t sset2;
        pddlSetISetInit(&sset2);
        pddlBicliqueFindMaximal(&g, addBiclique, &sset2);
        int size2 = pddlSetISetSize(&sset2);
        assert(size == size2);
        for (int i = 0; i < size && i < size2; ++i){
            const pddl_iset_t *c1 = pddlSetISetGet(&sset, i);
            const pddl_iset_t *c2 = pddlSetISetGet(&sset2, i);
            assert(pddlSetISetFind(&sset, c2) >= 0);
            assert(pddlSetISetFind(&sset2, c1) >= 0);
        }
        //fprintf(stderr, "%d %d\n", size, size2);
        pddlSetISetFree(&sset2);

        for (int k = 0; k < size; ++k){
            const pddl_iset_t *biclique = pddlSetISetGet(&sset, k);

            // Check that biclique is indeed a non-induced biclique
            int csize = pddlISetSize(biclique);
            for (int ci = 0; ci < csize; ++ci){
                int n1 = pddlISetGet(biclique, ci);
                if (n1 >= 1000)
                    break;

                for (int cj = ci + 1; cj < csize; ++cj){
                    int n2 = pddlISetGet(biclique, cj);
                    if (n2 < 1000)
                        continue;
                    n2 -= 1000;
                    assert(pddlISetIn(n2, &g.node[n1]));
                }
            }
        }

        // Check that there are not any subsets
        for (int i = 0; i < size; ++i){
            const pddl_iset_t *biclique = pddlSetISetGet(&sset, i);
            for (int j = i + 1; j < size; ++j){
                const pddl_iset_t *biclique2 = pddlSetISetGet(&sset, j);
                assert(!pddlISetIsSubset(biclique, biclique2));
                assert(!pddlISetIsSubset(biclique2, biclique));
            }
        }

        pddlSetISetFree(&sset);

        pddlGraphSimpleFree(&g);
    }
}
