#pragma once

#include "var_scope.h"
#include "bytecode.h"



typedef struct nl_closure_t {
    scope* env;
    chunk* bc;
    bool env_taken;  // `true` if a `var_scope` has moved ownership to a closure, implying that closure must cleanup its `env` ptr AND the upvals are closed.
} closure;

void closure_del(void* c);
