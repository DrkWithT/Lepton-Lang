#pragma once

#include "shape.h"



typedef struct nl_shape_transitions_t {
    uint16_t* keys;         // parallel buffer of interned property name keys
    int32_t* dest_ids;      // parallel buffer of destination IDs for shape's parent-to-child links in `shape_manager`
    uint16_t entries_n;     // count of used SoA slots for keys (`uint16_t` since the VM only supports `u16::MAX` names per chunk / object)
    uint16_t entries_cap;   // capacity of used transition slots
} shape_transitions;

[[nodiscard]]
shape_transitions sht_new();

void sht_del(shape_transitions* sht);

void sht_add_transit(shape_transitions* sht, uint16_t key_id, int32_t dest_shape_id);

void sht_invalidate_transit(shape_transitions* sht, uint16_t key_id);

/**
 ### ABOUT

 This type encapsulates a VM-wide object that manages the memory and transitions of object layouts (shapes) which are separate from the object themselves. Shapes are just glorified dictionaries mapping keys to offsets for each object's property values. This enables inline caching of property accesses based on whether an object has a familiar runtime shape, allowing direct indexing of a property value in good cases.

 Note that IDs are mentioned interchangeably with indexes to an internal object buffer.
 */
typedef struct nl_shape_manager_t {
    dynbuf free_ids;                // free slot indexes for both shape and transition collections
    shape* realm;                   // entire shape population's buffer
    shape_transitions* transits;    // parallel array of shape ancestries (see `shape_manager.h, typedef shape_transition`)
    size_t cost;
    size_t gc_cost;
    int32_t next_id;
    int32_t max_id;
} shm;

[[nodiscard]]
shm shm_new(int32_t max_shapes, const uint16_t* gc_cost_ratio_pair);

void shm_del(shm* sm);

[[nodiscard]]
NL_CXX_INLINE bool shm_should_gc(const shm* sm) {
    return sm->cost >= sm->gc_cost;
}

[[nodiscard]]
int32_t shm_add_shape(shm* sm, const shape* moving_s);

void shm_del_shape(shm* sm, int32_t shid);

[[nodiscard]]
const shape* shm_get_shape(shm* sm, int32_t shid);

[[nodiscard]]
shape* shm_get_shape_mut(shm* sm, int32_t shid);

[[nodiscard]]
NL_CXX_INLINE const shape* shm_get_shape_unchecked(shm* sm, int32_t shid) {
    return sm->realm + shid;
}

[[nodiscard]]
NL_CXX_INLINE shape* shm_get_shape_mut_unchecked(shm* sm, int32_t shid) {
    return sm->realm + shid;
}

[[nodiscard]]
const shape_transitions* shm_get_shape_transits(const shm* sm, int32_t shid);

const shape_transitions* shm_add_shape_transit(shm* sm, int32_t shid, uint16_t key_id, int32_t child_shid);
