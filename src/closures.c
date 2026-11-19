#include "closures.h"



void closure_del(void* self) {
    auto closure_self = (closure*) self;

    if (closure_self->env_taken) {
        scope_del(closure_self->env);
        closure_self->env_taken = false;
    }
}
