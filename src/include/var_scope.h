#pragma once

#include "value.h"



typedef struct nl_var_ent_t {
    uint32_t id;    // offset / name ID
    uint16_t cid;   // variable's home chunk ID
} var_ent;

typedef struct nl_scope_info_t scope;

typedef struct nl_scope_info_t {
    const var_ent* vars;        // viewing window of `chunks[cid].vinfo`, has `nullptr` deleter
    scope* parent;              // parent scope in scopes pool
    value* vbase;               // variable window
    value* closed_uvbuf;        // closed upvalue buffer
    uint16_t cid;               // corresponding function's chunk ID (`cid`)
    uint16_t vars_n;            // count of valid slots in `vars`
    bool saved;                 // is `closed_uvbuf` used?
    bool proxy;                 // is this scope based on an existing, persistent scope (only differing in `vbase` address and that any closed upvalues are not freed through it)
} scope;

void scope_del(void* s);
