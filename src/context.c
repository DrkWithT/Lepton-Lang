#include <math.h>
#include "strs.h"
#include "context.h"
#include "closures.h"
#include "shape.h"
#include "table.h"



constexpr size_t nl_default_stack_size = 8192;
constexpr size_t nl_default_arg_regs_n = 255;
constexpr size_t nl_default_max_scope_depth = 100;
constexpr int32_t nl_ic_dud_offset = -1;
constexpr int32_t nl_ic_entry_windows[] = {
    [nl_ics_idle] = 0,
    [nl_ics_mono] = 1,
    [nl_ics_poly] = 4,
    [nl_ics_dead] = 0,
};
constexpr uint16_t nl_ic_leeways[] = {
    [nl_ics_idle] = 1,
    [nl_ics_mono] = 10, // ! allow 10 misses to quickly transition to PIC if needed.
    [nl_ics_poly] = 1000,
    [nl_ics_dead] = 40000
};



[[nodiscard]]
int32_t context_lookup_ic(ic* cache, int32_t reciever_shape_id) {
    const auto curr_ic_entry_window = nl_ic_entry_windows[cache->state];

    for (int32_t ic_entry_pos = 0; ic_entry_pos < curr_ic_entry_window; ic_entry_pos++) {
        if (cache->shape_ids[ic_entry_pos] == reciever_shape_id) {
            return cache->obj_offsets[ic_entry_pos];
        }
    }

    // ! Here, the IC has missed too much predictions. Update its state.
    if (cache->leeway-- < 1) {
        switch (cache->state) {
            case nl_ics_idle: {
                cache->state = nl_ics_mono;
                cache->leeway = nl_ic_leeways[nl_ics_mono];
            } break;
            case nl_ics_mono: {
                cache->state = nl_ics_poly;
                cache->leeway = nl_ic_leeways[nl_ics_poly];
            } break;
            default: {
                cache->state = nl_ics_dead;
                cache->leeway = nl_ic_leeways[nl_ics_dead];
            } break;
        }
    }

    return nl_ic_dud_offset;
}

[[nodiscard]]
NL_CXX_INLINE const shape_ent* context_lookup_shape_slot(const shape* target_shape, uint16_t key_id) {
    const shape_ent* const shape_entries = target_shape->entries.data;
    const auto shape_entry_n = target_shape->entries.n;

    for (uint32_t entry_pos = 0; entry_pos < shape_entry_n; entry_pos++) {
        if (shape_entries[entry_pos].key_sid == key_id) {
            return &shape_entries[entry_pos];
        }
    }

    return nullptr;
}



[[nodiscard]]
context context_new(program pg, size_t shape_capacity) {
    const auto vm_initial_chunk = (chunk*) dynbuf_get_mut(&pg.chunks, pg.entry_cid);
    const inst* vm_initial_ip = vm_initial_chunk->bc;
    const value* vm_initial_kp = vm_initial_chunk->kd;
    auto vm_initial_icp = vm_initial_chunk->icv;

    auto shapes = shm_new(shape_capacity, (uint16_t[]) { 3, 4 });

    {
        shape base_shape = {
            .entries = dynbuf_new(sizeof(shape_ent), 4, nullptr),
            .id = 0
        };

        if (shm_add_shape(&shapes, &base_shape) == -1) {
            NL_PANIC("Failed to intern blank object layout. This may be a bug", __FILE__, __LINE__)
        }
    }

    NL_ALLOC_N(nl_default_stack_size, value, vm_stack_buf, __FILE__, __LINE__)
    NL_ALLOC_N(nl_default_arg_regs_n, value, vm_arg_regs_buf, __FILE__, __LINE__)

    auto frame_stack = dynbuf_new(sizeof(context_frame), nl_default_max_scope_depth, dud_deleter);

    {
        context_frame top_frame = {
            .caller_base_ip = nullptr,
            .caller_ip = nullptr,
            .caller_kp = nullptr,
            .caller_icaches = nullptr,
            .caller_stack_base = vm_stack_buf,
            .closure_id = -1,
            .self_obj_id = -1,
            .cid = pg.entry_cid
        };

        dynbuf_push(&frame_stack, &top_frame);
    }

    return (context) {
        .pool_s = pg.pool_s,
        .pool_o = pg.pool_o,
        .pool_c = pg.pool_c,
        .shapes = shapes,
        .chunks = pg.chunks,
        .frames = frame_stack,
        .stack = vm_stack_buf,
        .gv = pg.gv,
        .arg_regs = vm_arg_regs_buf,
        .base_ip = vm_initial_ip,
        .ip = vm_initial_ip,
        .kp = vm_initial_kp,
        .icp = vm_initial_icp,
        .stack_bp = vm_stack_buf,
        .natives = pg.natives,
        .accr = { .b = 0, .tag = nl_vtag_nil },
        .stack_sz = nl_default_stack_size,
        .gv_cap = pg.gv_cap,
        .gv_len = pg.gv_len,
        .flags = nl_ctxf_pending,
        .curr_ic_id = 0
    };
}

void context_del(context* c) {
    ent_pool_del(&c->pool_s);
    ent_pool_del(&c->pool_o);
    ent_pool_del(&c->pool_c);

    shm_del(&c->shapes);

    dynbuf_del(&c->chunks);
    dynbuf_del(&c->frames);

    if (c->stack) {
        free(c->stack);
        c->stack = nullptr;
    }

    if (c->gv) {
        free(c->gv);
        c->gv = nullptr;
    }

    if (c->natives) {
        free(c->natives);
        c->natives = nullptr;
    }

    if (c->arg_regs) {
        free(c->arg_regs);
        c->arg_regs = nullptr;
    }
}

// ! IMPORTANT: Only used in `context_try_closurify`. 
[[nodiscard]]
upvalue context_mk_upvalue_from(context* c, uint16_t uv_cid, uint16_t uv_reg_slot) {
    context_frame* frame_it = dynbuf_last_mut(&c->frames);
    context_frame* const frame_base = c->frames.data;

    for (; frame_it->caller_stack_base != nullptr && frame_it->cid != uv_cid; frame_it--) {}

    if (!frame_it->caller_stack_base) {
        NL_PANIC("Invalid capture of variable could not be resolved here. This may be an implementation bug.", __FILE__, __LINE__)
    }

    return (upvalue) {
        .ref = frame_it->caller_stack_base + uv_reg_slot,
        .v = { .b = 0, .tag = nl_vtag_nil },
        .cid = frame_it->cid,
        .id = uv_reg_slot,
        .closed = false
    };
}

[[nodiscard]]
value context_try_closurify(context* c, uint16_t chunk_id) {
    chunk* closure_chunk = dynbuf_get_mut_unchecked(&c->chunks, chunk_id);
    const uint32_t* upvalue_metadata = closure_chunk->upval_entries;
    const auto upvalue_count = closure_chunk->upvals_n;
    NL_ALLOC_N(upvalue_count, upvalue, upval_buf, __FILE__, __LINE__)

    for (uint16_t upval_slot = 0; upval_slot < upvalue_count; upval_slot++) {
        const uint16_t upvalue_cid = upvalue_metadata[upval_slot] & 0x0000ffff;
        const uint16_t upvalue_slot = (upvalue_metadata[upval_slot] & 0xffff0000) >> 16;

        upval_buf[upval_slot] = context_mk_upvalue_from(c, upvalue_cid, upval_slot);
    }

    closure temp_closure = {
        .bc = closure_chunk,
        .upvals = upval_buf,
        .upvals_cap = upvalue_count,
        .cid = chunk_id,
        .persist = false
    };

    auto closure_handle = ent_pool_add(&c->pool_c, &temp_closure);

    if (closure_handle.id < 0) {
        return (value) { .b = 0, .tag = nl_vtag_nil };
    }

    return (value) {
        .n = closure_handle.id,
        .tag = nl_vtag_closure
    };
}

bool context_try_invoke(context* c, value callee, int32_t maybe_self_id) {
    if (callee.tag != nl_vtag_closure) {
        return false;
    }

    const chunk* const caller_chunk = dynbuf_get_unchecked(&c->chunks, ((const context_frame*) dynbuf_last_unchecked(&c->frames))->cid);
    closure* target_closure = ent_pool_get_mut_unchecked(&c->pool_c, callee.n).p;

    {
        context_frame target_frame = {
            .caller_base_ip = c->base_ip,
            // ! IMPORTANT: skip next 2 u16 operands of current call-opcode in the caller to avoid misinterpretation
            .caller_ip = c->ip + 1,
            .caller_kp = c->kp,
            .caller_icaches = c->icp,
            .caller_stack_base = c->stack_bp,
            .closure_id = callee.n,
            // ! IMPORTANT: Set dud ID for non-methods only.
            .self_obj_id = maybe_self_id,
            .cid = target_closure->cid
        };

        dynbuf_push(&c->frames, &target_frame);
    }

    chunk* target_chunk = dynbuf_get_mut_unchecked(&c->chunks, target_closure->cid);

    c->base_ip = target_chunk->bc;
    c->ip = target_chunk->bc;
    c->kp = target_chunk->kd;
    c->icp = target_chunk->icv;
    c->stack_bp = c->stack_bp + caller_chunk->regs_n;

    return true;
}

[[nodiscard]]
entity_handle_mut context_create_with_trait(context* c, value trait_v, uint16_t initial_named_props_n) {
    int32_t trait_object_id = -1;

    if (trait_v.tag == nl_vtag_obj) {
        trait_object_id = ent_pool_get_mut(&c->pool_o, trait_v.n).id;
    }

    NL_ALLOC_N(initial_named_props_n, value, temp_pvals, __FILE__, __LINE__)
    NL_ALLOC_N(4, value, temp_ivals, __FILE__, __LINE__)

    metatable temp_object = {
        .pvals = temp_pvals,
        .ivals = temp_ivals,
        .shape_id = 0,
        .proto_obj_id = trait_object_id,
        .prop_n = 0,
        .prop_cap = initial_named_props_n,
        .item_n = 0,
        .item_cap = 4
    };

    return ent_pool_add(&c->pool_o, &temp_object);
}

[[nodiscard]]
bool context_try_invoke_method(context* c, value caller_obj, int16_t prop_method_id) {
    if (caller_obj.tag != nl_vtag_obj) {
        return false;
    }

    ic* const buddy_ic = &c->icp[c->curr_ic_id];
    metatable* const target_object = ent_pool_get_mut_unchecked(&c->pool_o, caller_obj.n).p;
    const auto object_id = caller_obj.n;
    const auto object_shape_id = target_object->shape_id;
    const auto ic_result_offset = context_lookup_ic(buddy_ic, object_shape_id);

    // ! IC Hits, so we're in "La La Land". Just use the cached offset according to the object's layout vs. a full shape lookup.
    if (ic_result_offset != nl_ic_dud_offset) {
        return context_try_invoke(c, target_object->pvals[ic_result_offset], object_id);
    }

    // ! Otherwise, we need to update the now invalidated IC with this fresh miss as entry 1. This LRU behavior MAY avoid future unlucky property lookups.
    const shape* const target_shape = shm_get_shape_unchecked(&c->shapes, object_shape_id);
    const auto prop_entry = context_lookup_shape_slot(target_shape, prop_method_id);

    if (!prop_entry) {
        return false;
    }

    const auto prop_offset = prop_entry->offset;

    buddy_ic->shape_ids[0] = object_shape_id;
    buddy_ic->obj_offsets[0] = prop_offset;
    buddy_ic->recv_ids[0] = object_id;

    return context_try_invoke(c, target_object->pvals[prop_offset], object_id);
}

void context_seal_object(context* c, value object_v) {
    // ! BCC only emits sealing of properties for objects via `bcc_emit_new_table()`, so this unchecked lookup is sound.
    metatable* const target_object = ent_pool_get_mut_unchecked(&c->pool_o, object_v.n).p;
    shape* const target_object_shape = shm_get_shape_mut(&c->shapes, target_object->shape_id);

    shape_ent* const shape_entries = target_object_shape->entries.data;
    const uint16_t shape_entry_n = target_object_shape->entries.n;

    for (uint16_t prop_offset = 0; prop_offset < shape_entry_n; prop_offset++) {
        shape_entries[prop_offset].meta &= (uint8_t) (~nl_prop_may_del);
    }
}

void context_init_property(context* c, value object_v, uint16_t key_id, value initial_v) {
    // ! BCC only emits the opcode using this method in freshly created object expressions, so an unchecked lookup is sound.
    metatable* const target_object = ent_pool_get_mut_unchecked(&c->pool_o, object_v.n).p;
    shape_transitions* target_shape_transitions_list = shm_get_mut_shape_transits(&c->shapes, target_object->shape_id);

    {
        const auto target_transition_n = target_shape_transitions_list->entries_n;

        for (uint16_t transit_pos = 0; transit_pos < target_transition_n; transit_pos++) {
            if (target_shape_transitions_list->keys[transit_pos] == key_id) {
                // ! Shape transitions always start from blank {}, and each new transition in additive order is placed for each new property. Also, the number of properties added in such an order is already known to BCC. We only need to update the next property slot for this object.
                target_object->shape_id = target_shape_transitions_list->dest_ids[transit_pos];
                target_object->pvals[target_object->prop_n++] = initial_v;

                return;
            }
        }
    }

    shape* const target_object_shape = shm_get_shape_mut_unchecked(&c->shapes, target_object->shape_id);
    const auto duped_object_shape_next_pos = target_object_shape->entries.n;
    const auto duped_object_shape_cap = duped_object_shape_next_pos + 1;

    NL_ALLOC_N(duped_object_shape_cap, shape_ent, duped_shape_entries_raw, __FILE__, __LINE__)
    memcpy(duped_shape_entries_raw, target_object_shape->entries.data, duped_object_shape_next_pos * sizeof(shape_ent));

    dynbuf duped_shape_entries = {
        .data = duped_shape_entries_raw,
        .del_fn = nullptr,
        .item_size = sizeof(shape_ent),
        .cap = duped_object_shape_cap,
        .n = duped_object_shape_next_pos
    };

    {
        shape_ent incoming_prop = {
            .key_sid = key_id,
            .offset = duped_object_shape_next_pos,
            .meta = nl_prop_default
        };

        dynbuf_push(&duped_shape_entries, &incoming_prop);
    }

    // ! Add property slot of object and map it within the fresh shape which is interned.
    const auto fresh_shape_id = shm_add_shape(&c->shapes, 
        &(shape) {
            .entries = duped_shape_entries,
            .id = 0
        }
    );

    target_object->shape_id = fresh_shape_id;
    target_object->pvals[duped_object_shape_next_pos] = initial_v;

    sht_add_transit(target_shape_transitions_list, key_id, fresh_shape_id);
}

[[nodiscard]]
bool context_set_property(context* c, value object_v, uint16_t key_id, value v) {
    if (object_v.tag != nl_vtag_obj) {
        return false;
    }

    ic* const buddy_ic = &c->icp[c->curr_ic_id];
    metatable* const target_object = ent_pool_get_mut_unchecked(&c->pool_o, object_v.n).p;
    const auto object_id = object_v.n;
    const auto object_shape_id = target_object->shape_id;
    const auto ic_result_offset = context_lookup_ic(buddy_ic, object_shape_id);

    // ! IC Hits, so we're in "La La Land". Just use the cached offset according to the object's layout vs. a full shape lookup.
    if (ic_result_offset != nl_ic_dud_offset) {
        target_object->pvals[ic_result_offset] = v;

        return true;
    }

    // ! Otherwise, we need to update the now invalidated IC with this fresh miss as entry 1. This LRU behavior MAY avoid future unlucky property lookups.
    const shape* const target_shape = shm_get_shape_unchecked(&c->shapes, object_shape_id);
    const auto prop_entry = context_lookup_shape_slot(target_shape, key_id);

    if (!prop_entry) {
        return false;
    }

    const auto prop_offset = prop_entry->offset;

    target_object->pvals[prop_offset] = v;
    buddy_ic->shape_ids[0] = object_shape_id;
    buddy_ic->obj_offsets[0] = prop_offset;
    buddy_ic->recv_ids[0] = object_id;

    return true;
}

[[nodiscard]]
value* context_get_property(context* c, value object_v, uint16_t key_id) {
    if (object_v.tag != nl_vtag_obj) {
        return nullptr;
    }

    ic* const buddy_ic = &c->icp[c->curr_ic_id];
    metatable* const target_object = ent_pool_get_mut_unchecked(&c->pool_o, object_v.n).p;
    const auto object_id = object_v.n;
    const auto object_shape_id = target_object->shape_id;
    const auto ic_result_offset = context_lookup_ic(buddy_ic, object_shape_id);

    // ! IC Hits, so we're in "La La Land". Just use the cached offset according to the object's layout vs. a full shape lookup.
    if (ic_result_offset != nl_ic_dud_offset) {
        return &target_object->pvals[ic_result_offset];
    }

    // ! Otherwise, we need to update the now invalidated IC with this fresh miss as entry 1. This LRU behavior MAY avoid future unlucky property lookups.
    const shape* const target_shape = shm_get_shape_unchecked(&c->shapes, object_shape_id);
    const auto prop_entry = context_lookup_shape_slot(target_shape, key_id);

    if (!prop_entry) {
        return nullptr;
    }

    const auto prop_offset = prop_entry->offset;

    buddy_ic->shape_ids[0] = object_shape_id;
    buddy_ic->obj_offsets[0] = prop_offset;
    buddy_ic->recv_ids[0] = object_id;

    return &target_object->pvals[prop_offset];
}

[[nodiscard]]
bool context_set_computed_property(context* c, value object_v, value key_v, value v) {
    return false; // todo: implement
}

[[nodiscard]]
value* context_get_computed_property(context* c, value object_v, value key_v) {
    return nullptr; // todo: implement
}

[[nodiscard]]
bool context_convert_to_bool(const context* c, value v) {
    switch (v.tag) {
        case nl_vtag_nil: return false; break;
        case nl_vtag_bool: return v.b != 0; break;
        case nl_vtag_int: return v.n != 0; break;
        case nl_vtag_num: return v.f != 0.0f; break;
        case nl_vtag_str: return ((const string*) ent_pool_get_unchecked(&c->pool_s, v.n).p)->n > 0; break;
        case nl_vtag_obj: default: return true; break;
    }
}

[[nodiscard]]
int32_t context_convert_to_i32(const context* c, value v) {
    switch (v.tag) {
        case nl_vtag_bool: return v.b & 0x1;
        case nl_vtag_int: return v.n;
        case nl_vtag_num: return (int32_t) floorf(v.f);
        default: return 0; // ! case should not be used
    }
}

[[nodiscard]]
float context_convert_to_f32(const context* c, value v) {
    switch (v.tag) {
        case nl_vtag_bool: return (float) (v.b & 0x1);
        case nl_vtag_int: return (float) v.n;
        case nl_vtag_num: return v.f;
        default: return NAN;
    }
}

[[nodiscard]]
bool context_test_equality(const context* c, value lhs, value rhs) {
    if (lhs.tag != rhs.tag) {
        if ((lhs.tag == nl_vtag_int && rhs.tag == nl_vtag_num) || (lhs.tag == nl_vtag_num && rhs.tag == nl_vtag_int)) {
            return context_convert_to_f32(c, lhs) == context_convert_to_f32(c, rhs);
        }

        return false;
    }

    switch (lhs.tag) {
        case nl_vtag_nil: return true;
        case nl_vtag_bool: return lhs.b == rhs.b;
        case nl_vtag_int: return lhs.n == rhs.n;
        case nl_vtag_num: return lhs.f == rhs.f;
        case nl_vtag_str: case nl_vtag_obj: default: return lhs.n == rhs.n;
    }
}

[[nodiscard]]
const value* context_get_upvalue(const context* c, uint16_t uv_id) {
    const closure* const callee = ent_pool_get_unchecked(&c->pool_c, ((const context_frame*) dynbuf_last_unchecked(&c->frames))->closure_id).p;

    return callee->upvals[uv_id].ref;
}

void context_set_upvalue(context* c, uint16_t uv_id, value v) {
    closure* const callee = ent_pool_get_mut(&c->pool_c, ((const context_frame*) dynbuf_last_unchecked(&c->frames))->closure_id).p;

    callee->upvals[uv_id].ref[0] = v;
}

void context_close_upvalues(context* c, int32_t closure_id) {
    closure* const callee = ent_pool_get_mut_unchecked(&c->pool_c, closure_id).p;

    if (callee->persist || !callee->upvals) {
        return;
    }

    upvalue* callee_upvalue_it = callee->upvals;
    upvalue* const callee_upvalue_end = callee->upvals + callee->upvals_cap;

    for (; callee_upvalue_it != callee_upvalue_end; callee_upvalue_it++) {
        callee_upvalue_it->v = *callee_upvalue_it->ref;
        callee_upvalue_it->ref = &callee_upvalue_it->v;
        callee_upvalue_it->closed = true;
    }

    callee->persist = true;
}
