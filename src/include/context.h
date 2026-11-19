#pragma once

#include "bytecode.h"



/**
 ### ABOUT

 This data type encapsulates the entire runtime state of the Neu language. It is meant to hold VM state for managing memory, execution location, call frames, etc. Also, native functions which are added to the interpreter MUST take in a pointer to this state.
 */
typedef struct nl_context_t {
    ent_pool pool_s;    // interned strings
    ent_pool pool_o;    // objects (metatables)
    ent_pool pool_c;    // closures
    dynbuf chunks;
    dynbuf scopes;      // stack-like buffer where scopes
    value* stack;       // all register slots
    value* gv;          // global values buffer
    value* arg_regs;    // scratch registers for up to 255 argument values per callee
    const uint8_t* ip;  // points to first byte of encoded instruction
    const value* kp;    // points to chunk constants
    ic* icp;            // points to chunk ICs
    uint32_t stack_sz;
    uint32_t scope_pos; // scope index to `context::scopes`
    uint16_t gv_len;    // global value count (determined at bytecode compilation)
    uint16_t gv_cap;    // global value capacity (determined at bytecode compilation)
} context;

[[nodiscard]]
context context_new(program pg);

void context_del(context* c);

bool context_try_invoke(context* c, value callee, uint8_t arity);

[[nodiscard]]
value context_get_foreign_variable(context* c, uint16_t cid, uint16_t local_reg, value v);

[[nodiscard]]
bool context_set_foreign_variable(context* c, uint16_t uv_cid, uint16_t uv_local_reg, value v);
