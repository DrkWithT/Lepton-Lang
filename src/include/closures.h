#pragma once

#include "var_scope.h"
#include "bytecode.h"



typedef struct nl_closure_t {
    scope* env;
    chunk* bc;
} closure;
