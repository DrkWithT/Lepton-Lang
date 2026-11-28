#include "shape_manager.h"



constexpr uint16_t nl_default_sht_cap = 4;
constexpr int32_t nl_dud_shape_id = -1;



[[nodiscard]]
shape_transitions sht_new() {
    return (shape_transitions) {
        .keys = calloc(nl_default_sht_cap, sizeof(uint16_t)),
        .dest_ids = calloc(nl_default_sht_cap, sizeof(int32_t)),
        .entries_n = 0,
        .entries_cap = nl_default_sht_cap
    };
}

void sht_del(shape_transitions* sht) {
    if (sht->keys) {
        free(sht->keys);
        sht->keys = nullptr;
    }

    if (sht->dest_ids) {
        free(sht->dest_ids);
        sht->dest_ids = nullptr;
    }
}

void sht_add_transit(shape_transitions* sht, uint16_t key_id, int32_t dest_shape_id) {
    const auto insert_pos = sht->entries_n;
    const auto curr_cap = sht->entries_cap;
    
    if (insert_pos >= curr_cap) {
        const auto new_cap = (sht->entries_cap * 3) / 2;
        uint16_t* new_keys = realloc(sht->keys, new_cap * sizeof(uint16_t));
        int32_t* new_dests = realloc(sht->dest_ids, new_cap * sizeof(int32_t));

        if (!new_keys || !new_dests) {
            NL_PANIC("Failed to grow shape transitions list here.", __FILE__, __LINE__)
        }

        sht->keys = new_keys;
        sht->dest_ids = new_dests;
        sht->entries_cap = new_cap;
    }

    sht->keys[insert_pos] = key_id;
    sht->dest_ids[insert_pos] = dest_shape_id;
    sht->entries_n++;
}

void sht_invalidate_transit(shape_transitions* sht, uint16_t key_id) {
    const auto last_key_pos = sht->entries_n;
    uint16_t key_pos = 0;

    for (; key_pos < last_key_pos; key_pos++) {
        if (sht->keys[key_pos] == key_id) {
            sht->dest_ids[key_pos] = nl_dud_shape_id;
            break;
        }
    }
}



[[nodiscard]]
int32_t shm_gen_id(shm* sm) {
    if (dynbuf_empty(&sm->free_ids)) {
        if (sm->next_id > sm->max_id) {
            NL_PANIC("Exhausted IDs in shape manager!", __FILE__, __LINE__)
        }

        return sm->next_id++;
    }

    const auto reused_free_id = (const int32_t*) dynbuf_last(&sm->free_ids);

    dynbuf_pop(&sm->free_ids);

    return *reused_free_id;
}

[[nodiscard]]
shm shm_new(int32_t max_shapes, const uint16_t* gc_cost_ratio_pair) {
    return (shm) {
        .realm = calloc(max_shapes, sizeof(shape)),
        .free_ids = dynbuf_new(sizeof(int32_t), max_shapes / 2, nullptr),
        .transits = calloc(max_shapes, sizeof(shape_transitions)),
        .cost = 0,
        .gc_cost = (sizeof(shape) * (gc_cost_ratio_pair[0])) / gc_cost_ratio_pair[1],
        .next_id = 0,
        .max_id = max_shapes
    };
}

void shm_del(shm* sm) {
    for (int32_t shape_id = 0; shape_id < sm->next_id; shape_id++) {
        sht_del(&sm->transits[shape_id]);

        const auto target_shape = &sm->realm[shape_id];
        
        // ! The GC might have reset this shape's ID to -1 as a quick marker upon reaping, so do this check to avoid repeat cleanup work.
        if (target_shape->id != nl_dud_shape_id) {
            dynbuf_del(&target_shape->entries);
            target_shape->id = nl_dud_shape_id;
        }
    }

    dynbuf_del(&sm->free_ids);
}

[[nodiscard]]
int32_t shm_add_shape(shm* sm, const shape* moving_s) {
    const auto next_slot = shm_gen_id(sm);
    auto dest_region = sm->realm + next_slot;
    auto dest_shape_transits_region = sm->transits + next_slot;

    memcpy(dest_region, moving_s, sizeof(shape));
    dest_region->id = next_slot;
    *dest_shape_transits_region = sht_new();

    sm->cost += sizeof(shape);

    return next_slot;
}

void shm_del_shape(shm* sm, int32_t shid) {
    const auto target_shape_p = sm->realm + shid;
    const auto target_transits_entry = sm->transits + shid;

    // ! 1. Destroy shape and its transitions at the slot.
    sht_del(target_transits_entry);
    dynbuf_del(&target_shape_p->entries);
    target_shape_p->id = nl_dud_shape_id;

    sm->cost -= sizeof(shape);

    // ! 2. Track the vacant slot for reuse.
    dynbuf_push(&sm->free_ids, (int32_t[]) { shid });
}

[[nodiscard]]
const shape* shm_get_shape(shm* sm, int32_t shid) {
    if (shid <= nl_dud_shape_id || shid > sm->max_id) {
        return nullptr;
    }

    return sm->realm + shid;
}

[[nodiscard]]
shape* shm_get_shape_mut(shm* sm, int32_t shid) {
    if (shid <= nl_dud_shape_id || shid > sm->max_id) {
        return nullptr;
    }

    return sm->realm + shid;
}

[[nodiscard]]
const shape_transitions* shm_get_shape_transits(const shm* sm, int32_t shid) {
    if (shid <= nl_dud_shape_id || shid >= sm->max_id) {
        return nullptr;
    }

    return sm->transits + shid;
}

[[nodiscard]]
shape_transitions* shm_get_mut_shape_transits(shm* sm, int32_t shid) {
    if (shid <= nl_dud_shape_id || shid >= sm->max_id) {
        return nullptr;
    }

    return sm->transits + shid;
}

const shape_transitions* shm_add_shape_transit(shm* sm, int32_t shid, uint16_t key_id, int32_t child_shid) {
    if (shid <= nl_dud_shape_id || shid > sm->max_id) {
        return nullptr;
    }

    return sm->transits + shid;
}
