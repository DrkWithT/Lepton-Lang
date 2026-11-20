#include <stdlib.h>
#include "var_scope.h"



void scope_del(void *s) {
    auto scope_self = (scope*)s;

    if (scope_self->proxy || scope_self->mode != nl_sc_mode_closed) {
        return;
    }

    if (!scope_self->closed_uvbuf) {
        return;
    }

    free(scope_self->closed_uvbuf);
    scope_self->closed_uvbuf = nullptr;
}
