/***
 * Copyright (c)2026 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#include "pddl/pddl.h"
#include "test.h"
#include <assert.h>

/* Returns true if NAME is one of the space-separated words of LIST. */
static int hasWord(const char *list, const char *name)
{
    size_t len = strlen(name);
    const char *s = list;
    while ((s = strstr(s, name)) != NULL){
        if ((s == list || s[-1] == ' ') && (s[len] == '\0' || s[len] == ' '))
            return 1;
        s += len;
    }
    return 0;
}

#define CHECK_FEATURE(INFO, NAME, ENABLED) \
    assert(hasWord((INFO)->features, (NAME)) == (ENABLED))

#ifdef PDDL_SQLITE
# define HAVE_SQLITE 1
#else
# define HAVE_SQLITE 0
#endif
#ifdef PDDL_CLIQUER
# define HAVE_CLIQUER 1
#else
# define HAVE_CLIQUER 0
#endif
#ifdef PDDL_CUDD
# define HAVE_CUDD 1
#else
# define HAVE_CUDD 0
#endif
#ifdef PDDL_BLISS
# define HAVE_BLISS 1
#else
# define HAVE_BLISS 0
#endif
#ifdef PDDL_LP
# define HAVE_LP 1
#else
# define HAVE_LP 0
#endif
#ifdef PDDL_CPLEX
# define HAVE_CPLEX 1
#else
# define HAVE_CPLEX 0
#endif
#ifdef PDDL_CPLEX_ONLY_API
# define HAVE_CPLEX_ONLY_API 1
#else
# define HAVE_CPLEX_ONLY_API 0
#endif
#ifdef PDDL_CPOPTIMIZER
# define HAVE_CPOPTIMIZER 1
#else
# define HAVE_CPOPTIMIZER 0
#endif
#ifdef PDDL_GUROBI
# define HAVE_GUROBI 1
#else
# define HAVE_GUROBI 0
#endif
#ifdef PDDL_GUROBI_ONLY_API
# define HAVE_GUROBI_ONLY_API 1
#else
# define HAVE_GUROBI_ONLY_API 0
#endif
#ifdef PDDL_HIGHS
# define HAVE_HIGHS 1
#else
# define HAVE_HIGHS 0
#endif
#ifdef PDDL_MINIZINC
# define HAVE_MINIZINC 1
#else
# define HAVE_MINIZINC 0
#endif
#ifdef PDDL_DYNET
# define HAVE_DYNET 1
#else
# define HAVE_DYNET 0
#endif
#ifdef PDDL_CLINGO
# define HAVE_CLINGO 1
#else
# define HAVE_CLINGO 0
#endif
#ifdef PDDL_CADICAL
# define HAVE_CADICAL 1
#else
# define HAVE_CADICAL 0
#endif
#ifdef PDDL_MIMALLOC
# define HAVE_MIMALLOC 1
#else
# define HAVE_MIMALLOC 0
#endif

/*
 * pddlBuildInfo() has all members set, its feature list and debug flag
 * agree with the PDDL_* macros of pddl/config.h, and pddlBuildInfoPrint()
 * writes one TOML "key = value" line per key, starting with the version
 * and ending with the features array.
 */
TEST_ONCE(build_info)
{
    const pddl_build_info_t *info = pddlBuildInfo();
    assert(info != NULL);
    assert(info->target != NULL);
    assert(info->cc != NULL);
    assert(info->cc_version != NULL);
    assert(info->cxx != NULL);
    assert(info->cxx_version != NULL);
    assert(info->cflags != NULL);
    assert(info->cxxflags != NULL);
    assert(info->debug != NULL);
    assert(info->profil != NULL);
    assert(info->features != NULL);
    assert(info->lto != NULL);
    assert(info->march != NULL);
    assert(info->disable_hardening != NULL);
    assert(info->static_link != NULL);

    // The features are separated by single spaces
    size_t flen = strlen(info->features);
    if (flen > 0){
        assert(info->features[0] != ' ');
        assert(info->features[flen - 1] != ' ');
        assert(strstr(info->features, "  ") == NULL);
    }

    CHECK_FEATURE(info, "sqlite", HAVE_SQLITE);
    CHECK_FEATURE(info, "cliquer", HAVE_CLIQUER);
    CHECK_FEATURE(info, "cudd", HAVE_CUDD);
    CHECK_FEATURE(info, "bliss", HAVE_BLISS);
    CHECK_FEATURE(info, "lp", HAVE_LP);
    CHECK_FEATURE(info, "cplex", HAVE_CPLEX);
    CHECK_FEATURE(info, "cplex_only_api", HAVE_CPLEX_ONLY_API);
    CHECK_FEATURE(info, "cpoptimizer", HAVE_CPOPTIMIZER);
    CHECK_FEATURE(info, "gurobi", HAVE_GUROBI);
    CHECK_FEATURE(info, "gurobi_only_api", HAVE_GUROBI_ONLY_API);
    CHECK_FEATURE(info, "highs", HAVE_HIGHS);
    CHECK_FEATURE(info, "minizinc", HAVE_MINIZINC);
    CHECK_FEATURE(info, "dynet", HAVE_DYNET);
    CHECK_FEATURE(info, "clingo", HAVE_CLINGO);
    CHECK_FEATURE(info, "cadical", HAVE_CADICAL);
    CHECK_FEATURE(info, "mimalloc", HAVE_MIMALLOC);

#ifdef PDDL_DEBUG
    assert(strcmp(info->debug, "yes") == 0);
#else
    assert(strcmp(info->debug, "yes") != 0);
#endif

    static const char *keys[] = {
        "version", "commit", "target", "cc", "cc_version", "cxx",
        "cxx_version", "cflags", "cxxflags", "debug", "profil", "lto",
        "march", "disable_hardening", "static", "features",
    };
    const int keys_size = sizeof(keys) / sizeof(keys[0]);

    FILE *fout = tmpfile();
    assert(fout != NULL);
    pddlBuildInfoPrint(fout);
    rewind(fout);
    char line[16 * 1024];
    int num_lines = 0;
    while (fgets(line, sizeof(line), fout) != NULL){
        size_t len = strlen(line);
        assert(len > 0 && line[len - 1] == '\n');
        assert(num_lines < keys_size);
        const char *key = keys[num_lines];
        size_t klen = strlen(key);
        assert(strncmp(line, key, klen) == 0);
        if (strcmp(key, "features") == 0){
            assert(strncmp(line + klen, " = [", 4) == 0);
            assert(line[len - 2] == ']');
        }else{
            assert(strncmp(line + klen, " = \"", 4) == 0);
            assert(line[len - 2] == '"');
        }
        ++num_lines;
    }
    assert(num_lines == keys_size);
    fclose(fout);
}
