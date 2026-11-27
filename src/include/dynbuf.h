#pragma once

#include "basics.h"

typedef struct nl_dynbuf_t {
    void* data;
    nl_deleter_t del_fn;
    size_t item_size;
    uint32_t n;
    uint32_t cap;
} dynbuf;

[[nodiscard]]
dynbuf dynbuf_dud();

[[nodiscard]]
dynbuf dynbuf_new(size_t item_size, uint32_t init_cap, nl_deleter_t deleter);

void dynbuf_del(dynbuf* self);

NL_CXX_INLINE void dynbuf_lazy_pre_size(dynbuf* self, uint32_t n) {
    self->n = (n > self->cap) ? self->cap : n;
}

[[nodiscard]]
NL_CXX_INLINE const void* dynbuf_data(const dynbuf* self) {
    return self->data;
}

[[nodiscard]]
NL_CXX_INLINE bool dynbuf_empty(const dynbuf* self) {
    return !self->data || self->n == 0;
}

[[nodiscard]]
NL_CXX_INLINE size_t dynbuf_size(const dynbuf* self) {
    return self->n;
}

void* dynbuf_push(dynbuf* self, const void* src);

[[maybe_unused]]
size_t dynbuf_pop(dynbuf* self);

[[nodiscard]]
NL_CXX_INLINE const void* dynbuf_last(const dynbuf* self) {
    if (dynbuf_empty(self)) {
        return nullptr;
    }

    return (const uint8_t*) self->data + (self->n - 1) * self->item_size;
}

[[nodiscard]]
NL_CXX_INLINE void* dynbuf_last_mut(dynbuf* self) {
    if (dynbuf_empty(self)) {
        return nullptr;
    }

    return (uint8_t*) self->data + (self->n - 1) * self->item_size;
}

[[nodiscard]]
NL_CXX_INLINE const void* dynbuf_last_unchecked(const dynbuf* self) {
    return (const uint8_t*) self->data + (self->n - 1) * self->item_size;
}

[[nodiscard]]
NL_CXX_INLINE const void* dynbuf_get(const dynbuf* self, uint32_t pos) {
    if (pos >= self->n) {
        return nullptr;
    }

    return (const uint8_t*) self->data + pos * self->item_size;
}

[[nodiscard]]
NL_CXX_INLINE void* dynbuf_get_mut(dynbuf* self, uint32_t pos) {
    if (pos >= self->n) {
        return nullptr;
    }

    return (uint8_t*) self->data + pos * self->item_size;
}

[[nodiscard]]
NL_CXX_INLINE const void* dynbuf_get_unchecked(const dynbuf* self, uint32_t pos) {
    return (const uint8_t*) self->data + pos * self->item_size;
}

[[nodiscard]]
NL_CXX_INLINE void* dynbuf_get_mut_unchecked(dynbuf* self, uint32_t pos) {
    return (uint8_t*) self->data + pos * self->item_size;
}
