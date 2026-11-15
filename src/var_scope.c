#include <stdlib.h>
#include "var_scope.h"



void scope_del(void *s) {
    auto scope_self = (scope*)s;

    if (scope_self->closed_uvbuf){
        free(scope_self->closed_uvbuf);
        scope_self->closed_uvbuf = nullptr;
    }
}
