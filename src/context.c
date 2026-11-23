#include <math.h>
#include "strs.h"
#include "context.h"
#include "closures.h"



constexpr size_t nl_default_stack_size = 8192;
constexpr size_t nl_default_arg_regs_n = 255;
constexpr size_t nl_default_max_scope_depth = 64;



[[nodiscard]]
context context_new(program pg) {
    const auto vm_initial_chunk = (chunk*) dynbuf_get_mut(&pg.chunks, pg.entry_cid);
    const uint8_t* vm_initial_ip = vm_initial_chunk->bc;
    const value* vm_initial_kp = vm_initial_chunk->kd;
    auto vm_initial_icp = vm_initial_chunk->icv;

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
            .cid = pg.entry_cid
        };

        dynbuf_push(&frame_stack, &top_frame);
    }

    return (context) {
        .pool_s = pg.pool_s,
        .pool_o = pg.pool_o,
        .pool_c = pg.pool_c,
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
        .flags = nl_ctxf_pending
    };
}

void context_del(context* c) {
    ent_pool_del(&c->pool_s);
    ent_pool_del(&c->pool_o);
    ent_pool_del(&c->pool_c);

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

bool context_try_invoke(context* c, value callee, uint8_t arity) {
    if (callee.tag != nl_vtag_closure) {
        return false;
    }

    const chunk* const caller_chunk = dynbuf_get(&c->chunks, ((const context_frame*) dynbuf_last(&c->frames))->cid);
    closure* target_closure = ent_pool_get_mut_unchecked(&c->pool_c, callee.n).p;

    {
        context_frame target_frame = {
            .caller_base_ip = c->base_ip,
            // ! IMPORTANT: skip next 2 u16 operands of current call-opcode in the caller to avoid misinterpretation
            .caller_ip = c->ip + 4,
            .caller_kp = c->kp,
            .caller_icaches = c->icp,
            .caller_stack_base = c->stack_bp,
            .closure_id = callee.n,
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
const value* context_get_upvalue(const context* c, uint16_t uv_cid, uint16_t uv_local_reg) {
    const closure* const callee = ent_pool_get_unchecked(&c->pool_c, ((const context_frame*) dynbuf_last(&c->frames))->closure_id).p;

    const upvalue* callee_upvalue_it = callee->upvals;
    const upvalue* const callee_upvalue_end = callee->upvals + callee->upvals_cap;

    for (; callee_upvalue_it != callee_upvalue_end; callee_upvalue_it++) {
        if (callee_upvalue_it->cid == uv_cid && callee_upvalue_it->id == uv_local_reg) {
            return callee_upvalue_it->ref;
        }
    }

    return nullptr;
}

[[nodiscard]]
bool context_set_upvalue(context* c, uint16_t uv_cid, uint16_t uv_local_reg, value v) {
    closure* const callee = ent_pool_get_mut(&c->pool_c, ((const context_frame*) dynbuf_last(&c->frames))->closure_id).p;

    upvalue* callee_upvalue_it = callee->upvals;
    upvalue* const callee_upvalue_end = callee->upvals + callee->upvals_cap;

    for (; callee_upvalue_it != callee_upvalue_end; callee_upvalue_it++) {
        if (callee_upvalue_it->cid == uv_cid && callee_upvalue_it->id == uv_local_reg) {
            *callee_upvalue_it->ref = v;
            return true;
        }
    }

    return false;
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
