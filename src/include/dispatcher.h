#pragma once

#include "context.h"



#if defined(__llvm__) && defined(__clang__) && __clang_major__ > 20
    #define TAILCALL [[clang::musttail]]
#elif __GNUC__ > 15
    #define TAILCALL [[gnu::musttail]]
#else
    #error "This compiler does not support tail call optimization required for the VM."
#endif

/**
 ### ABOUT

 Type alias for Neu's VM opcode handler. It takes the VM context to use interpreter state.
 */
typedef void(*nl_op_handler_t)(context* c);

[[nodiscard]]
uint16_t nl_execute_with_context(context* c);
