#pragma once

#include "bytecode.h"



typedef struct nl_upvalue_t {
    value* ref;
    value v;
    uint16_t cid;
    uint16_t id;
    bool closed;
} upvalue;

typedef struct nl_closure_t {
    upvalue* upvals;
    chunk* bc;
    uint16_t upvals_cap;
    uint16_t cid;
    bool persist;
} closure;

void closure_del(void* c);
