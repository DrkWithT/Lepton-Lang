#pragma once

#include "shape.h"
#include "value.h"



/**
 ### ABOUT

 Metatables are the basic "objects" of Neu. Unlike JS or Lua, named properties cannot be monkey-patched after full table initialization. This gives a guarantee that the layouts of tables are constant, so inline caches can often be hit for property accesses since the layout types (shapes) are consistent.
 */
typedef struct nl_metatable_t {
    shape* layout;
    value* pvals;
    value* ivals;
    uint32_t prop_n;        // property value count
    uint32_t prop_cap;      // property capacity
    uint32_t item_n;        // item value count
    uint32_t item_cap;      // item capacity
    int32_t ent_id;         // closure or native chunk ID
    bool is_native_or_closure;  // true for native trampoline chunks
} metatable;

void metatable_del(void* t);
