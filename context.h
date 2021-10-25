#ifndef __TEST_CONTEXT_H__
#define __TEST_CONTEXT_H__

#include <pddl/pddl.h>

struct context {
    pddl_files_t files;
    pddl_t pddl;
    pddl_strips_t strips;
};
typedef struct context context_t;

extern context_t context;

#endif /* __TEST_CONTEXT_H__ */
