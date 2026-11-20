#include <math.h>
#include "strs.h"
#include "context.h"
#include "closures.h"



constexpr size_t nl_default_stack_size = 8192;
constexpr size_t nl_default_arg_regs_n = 255;
constexpr size_t nl_default_max_scope_depth = 64;
constexpr size_t nl_default_scope_cap = 2048;



[[nodiscard]]
context context_new(program pg) {
    const auto vm_initial_chunk = (chunk*) dynbuf_get_mut(&pg.chunks, pg.entry_cid);
    const uint8_t* vm_initial_ip = vm_initial_chunk->bc;
    const value* vm_initial_kp = vm_initial_chunk->kd;
    auto vm_initial_icp = vm_initial_chunk->icv;

    NL_ALLOC_N(nl_default_stack_size, value, vm_stack_buf, __FILE__, __LINE__)
    NL_ALLOC_N(nl_default_arg_regs_n, value, vm_arg_regs_buf, __FILE__, __LINE__)

    auto vm_pool_sc = ent_pool_new(sizeof(scope), nl_default_scope_cap, scope_del); // ? managed scopes
    scope* vm_initial_scope_p = nullptr;

    {
        scope vm_initial_scope_frame = {
            .vars = vm_initial_chunk->vinfo.data,
            .parent = nullptr,
            .vbase = vm_stack_buf,
            .closed_uvbuf = nullptr,
            .last_stack_base = 0,
            .pool_id = 0,
            .cid = pg.entry_cid,
            .vars_n = vm_initial_chunk->vinfo.n,
            .mode = nl_sc_mode_normal,
            .proxy = false,
        };
        
        auto initial_scope_handle = ent_pool_add(&vm_pool_sc, &vm_initial_scope_frame);

        vm_initial_scope_p = (scope*) initial_scope_handle.p;
        vm_initial_scope_p->pool_id = initial_scope_handle.id;
    }

    return (context) {
        .pool_s = pg.pool_s,
        .pool_o = pg.pool_o,
        .pool_c = pg.pool_c,
        .chunks = pg.chunks,
        .curr_scope = vm_initial_scope_p,
        .stack = vm_stack_buf,
        .gv = pg.gv,
        .arg_regs = vm_arg_regs_buf,
        .base_ip = vm_initial_ip,
        .ip = vm_initial_ip,
        .kp = vm_initial_kp,
        .icp = vm_initial_icp,
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

[[nodiscard]]
int32_t context_try_closurify(context* c, uint16_t chunk_id) {
    ; // todo: implement
}

// ! IMPORTANT: Expects callee `value` to contain an object ID of a closure which contains the chunk ID.
bool context_try_invoke(context* c, value callee, uint8_t arity) {
    if (callee.tag != nl_vtag_obj) {
        return false;
    }

    const auto curr_env = c->curr_scope;
    const auto curr_chunk = (chunk*) dynbuf_get_mut(&c->chunks, curr_env->cid);
    auto closure_handle = ent_pool_get_mut(&c->pool_c, callee.n);
    auto closure_ref = (closure*) closure_handle.p;

    if (!closure_ref) {
        return false;
    }

    chunk* const closure_chunk = closure_ref->bc;

    {
        scope closure_proxy_scope = *closure_ref->env;

        // todo: add `nl_op_mkcls_ret` bytecode op which runs upon closure return for functions that have function expressions anywhere in a returned value
        // ? 1. Add BCC hint bitflag for detection of sub-expressions in returns.
        // ? 2. Update BCC to emit this opcode just before finishing a lambda's code IFF there was a (proxy) returned lambda anywhere in any return.
        // ? 3. Implement context helper method for `mk_cls_ret` opcode.
        if (!closure_ref->env_taken) {
            closure_proxy_scope.vbase = c->stack + curr_env->last_stack_base + curr_chunk->regs_n;
            closure_proxy_scope.last_stack_base = curr_env->last_stack_base + curr_chunk->regs_n;
        } // todo: lifted closures should allocate slots for all their chunk registers: `regs_n`

        closure_proxy_scope.proxy = true;

        auto next_head_scope_handle = ent_pool_add(&c->pool_sc, &closure_proxy_scope);

        if (next_head_scope_handle.id < 0) {
            return false;
        }

        auto next_head_scope = (scope*) next_head_scope_handle.p;

        next_head_scope->parent = curr_env;
        next_head_scope->pool_id = next_head_scope_handle.id;
        c->curr_scope = next_head_scope;
    }

    c->base_ip = closure_chunk->bc;
    c->ip = closure_chunk->bc;
    c->kp = closure_chunk->kd;
    c->icp = closure_chunk->icv;

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
const value* context_get_foreign_variable(const context* c, uint16_t cid, uint16_t local_reg) {
    auto scope_it = (const scope*) c->curr_scope;

    scope_it = scope_it->parent;

    // ? Bubble up scopes until the global scope is reached: search until a corresponding one to the function is found. Then try lookup of the local slot / register.
    for (; scope_it != nullptr && scope_it->cid != cid; scope_it = scope_it->parent) {}

    const var_ent* scope_var_entry_it = scope_it->vars;
    const var_ent* const scope_var_entry_end = scope_it->vars + scope_it->vars_n;

    for (; scope_var_entry_it < scope_var_entry_end; scope_var_entry_it++) {
        if (scope_var_entry_it->id == local_reg) {
            return scope_it->vbase + local_reg;
        }
    }

    return nullptr;
}

[[nodiscard]]
bool context_set_foreign_variable(context* c, uint16_t uv_cid, uint16_t uv_local_reg, value v) {
    auto scope_it = (scope*) c->curr_scope;

    scope_it = scope_it->parent;

    // ? Bubble up scopes until the global scope is reached: search until a corresponding one to the function is found. Then try lookup of the local slot / register.
    for (; scope_it != nullptr && scope_it->cid != uv_cid; scope_it = scope_it->parent) {}

    const var_ent* scope_var_entry_it = scope_it->vars;
    const var_ent* const scope_var_entry_end = scope_it->vars + scope_it->vars_n;

    for (; scope_var_entry_it != scope_var_entry_end; scope_var_entry_it++) {
        if (scope_var_entry_it->id == uv_local_reg) {
            scope_it->vbase[uv_local_reg] = v;
            return true;
        }
    }

    return false;
}
