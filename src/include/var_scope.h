#pragma once

#include "value.h"



typedef struct nl_var_ent_t {
    uint32_t id;    // offset / name ID
    uint16_t cid;   // variable's home chunk ID
} var_ent;

typedef enum nl_scope_mode_t : uint8_t {
    nl_sc_mode_normal,  // no vars captured, just locals
    nl_sc_mode_opened,  // on-stack vars captured, but not persistent yet
    nl_sc_mode_closed,  // env-vars captured and persisted in closure pool (heap)
} scope_mode;

typedef struct nl_scope_info_t scope;

typedef struct nl_scope_info_t {
    const var_ent* vars;        // viewing window of `chunks[cid].vinfo`, has `nullptr` deleter
    scope* parent;              // parent scope in scopes pool
    value* vbase;               // variable window
    value* closed_uvbuf;        // closed upvalue buffer
    uint32_t last_stack_base;   // caller's base register offset
    int32_t pool_id;
    uint16_t cid;               // corresponding function's chunk ID (`cid`)
    uint16_t vars_n;            // count of valid slots in `vars`
    scope_mode mode;            // see `enum nl_scope_mode_t`
    bool proxy;
} scope;

void scope_del(void* s);
