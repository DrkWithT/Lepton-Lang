#include <stdlib.h>
#include "var_scope.h"



void scope_del(void *s) {
    auto scope_self = (scope*)s;

    if (scope_self->proxy) {
        return;
    }

    if (!scope_self->closed_uvbuf) {
        return;
    }

    free(scope_self->closed_uvbuf);
    scope_self->closed_uvbuf = nullptr;
}
