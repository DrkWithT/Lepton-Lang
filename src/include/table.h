#pragma once

#include "value.h"



/**
 ### ABOUT

 Metatables are the basic "objects" of Neu. Unlike JS or Lua, named properties cannot be monkey-patched after full table initialization, as they're sealed then. This gives a guarantee that the layouts of tables are constant, so inline caches are invalidated less for property accesses.
 */
typedef struct nl_metatable_t {
    value* pvals;
    value* ivals;
    int32_t shape_id;
    int32_t proto_obj_id;
    uint32_t prop_n;        // property value count
    uint32_t prop_cap;      // property capacity
    uint32_t item_n;        // item value count
    uint32_t item_cap;      // item capacity
} metatable;

void metatable_del(void* t);
