#include "closures.h"



void closure_del(void* self) {
    auto closure_self = (closure*) self;

    if (closure_self->upvals) {
        free(closure_self->upvals);
        closure_self->upvals = nullptr;
    }
}
