#include "table.h"



void metatable_del(void* t) {
    auto table_self = (metatable*) t;

    free(table_self->ivals);
    table_self->ivals = nullptr;
    free(table_self->pvals);
    table_self->pvals = nullptr;
    table_self->layout = nullptr;
}
