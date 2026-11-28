#include <math.h>
#include "dispatcher.h"



void op_nop(context* c);
void op_store_nil(context* c);
void op_store_bool(context* c);
void op_swap_r(context* c);
void op_loadi_u16(context* c);
void op_mov_rk(context* c);
void op_mov_rr(context* c);
void op_mov_rg(context* c);
void op_sto_arg(context* c);
void op_ld_arg(context* c);
void op_ld_upval(context* c);
void op_sto_upval(context* c);
void op_close_upvals(context* c);
void op_use_ic(context* c);
void op_inst_trait(context* c);
void op_seal_obj(context* c);
void op_init_prop(context* c);
void op_set_prop(context* c);
void op_get_prop(context* c);
void op_set_item(context* c);
void op_get_item(context* c);
void op_ref_self(context* c);
void op_ld_accr(context* c);
void op_test_nil(context* c);
void op_neg_num(context* c);
void op_neg_log(context* c);
void op_mod(context* c);
void op_mul(context* c);
void op_div(context* c);
void op_add(context* c);
void op_sub(context* c);
void op_lt(context* c);
void op_lte(context* c);
void op_gt(context* c);
void op_gte(context* c);
void op_eq(context* c);
void op_ne(context* c);
void op_jmp_not_nil(context* c);
void op_jmp_if(context* c);
void op_jmp_else(context* c);
void op_jmp(context* c);
void op_mk_closure(context* c);
void op_call(context* c);
void op_call_n(context* c);
void op_call_method(context* c);
void op_ret(context* c);

static const nl_op_handler_t op_handlers[] = {
    op_nop,
    op_store_nil,
    op_store_bool,
    op_swap_r,
    op_loadi_u16,
    op_mov_rk,
    op_mov_rr,
    op_mov_rg,
    op_sto_arg,
    op_ld_arg,
    op_ld_upval,
    op_sto_upval,
    op_close_upvals,
    op_use_ic,
    op_inst_trait,
    op_seal_obj,
    op_init_prop,
    op_set_prop,
    op_get_prop,
    op_set_item,
    op_get_item,
    op_ref_self,
    op_ld_accr,
    op_test_nil,
    op_neg_num,
    op_neg_log,
    op_mod,
    op_mul,
    op_div,
    op_add,
    op_sub,
    op_lt,
    op_lte,
    op_gt,
    op_gte,
    op_eq,
    op_ne,
    op_jmp_not_nil,
    op_jmp_if,
    op_jmp_else,
    op_jmp,
    op_mk_closure,
    op_call,
    op_call_n,
    op_call_method,
    op_ret,
};

static void dispatch_op(context* c) {
    TAILCALL
    return op_handlers[c->ip->o](c);
}

void op_nop(context* c) {
    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_store_nil(context* c) {
    const auto dest_reg = c->ip->a0;    
    c->stack_bp[dest_reg] = (value) { .b = 0, .tag = nl_vtag_nil };

    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_store_bool(context* c) {
    const auto dest_reg = c->ip->a0;

    const auto imm_arg = c->ip->a1;

    c->stack_bp[dest_reg] = (value) { .b = imm_arg & 1, .tag = nl_vtag_bool };
    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_swap_r(context* c) {
    const auto dest_reg = c->ip->a0;
    const auto src_reg = c->ip->a1;
    const auto temp_v = c->stack_bp[dest_reg];

    c->stack_bp[dest_reg] = c->stack_bp[src_reg];
    c->stack_bp[src_reg] = temp_v;
    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_loadi_u16(context* c) {
    const auto dest_reg = c->ip->a0;
    const int32_t imm_i32 = c->ip->a1;

    c->stack_bp[dest_reg] = (value) { .n = imm_i32, .tag = nl_vtag_int };
    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_mov_rk(context* c) {
    const auto dest_reg = c->ip->a0;
    const auto kv_id = c->ip->a1;
    
    c->stack_bp[dest_reg] = c->kp[kv_id];
    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_mov_rr(context* c) {
    const auto dest_reg = c->ip->a0;
    const auto src_reg = c->ip->a1;
    
    c->stack_bp[dest_reg] = c->stack_bp[src_reg];
    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_mov_rg(context* c) {
    const auto dest_reg = c->ip->a0;
    const auto gv_id = c->ip->a1;

    c->stack_bp[dest_reg] = c->gv[gv_id];
    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_sto_arg(context* c) {
    const auto arg_reg = c->ip->a0;
    const auto src_reg = c->ip->a1;

    c->arg_regs[arg_reg] = c->stack_bp[src_reg];
    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_ld_arg(context* c) {
    const auto dest_reg = c->ip->a0;
    const auto arg_reg = c->ip->a1;

    c->stack_bp[dest_reg] = c->arg_regs[arg_reg];
    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_ld_upval(context* c) {
    const auto dest_reg = c->ip->a0;
    const auto uv_id = c->ip->a1;

    auto checked_uv_ref = context_get_upvalue(c, uv_id);

    c->stack_bp[dest_reg] = *checked_uv_ref;
    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_sto_upval(context* c) {
    const auto dest_uv_id = c->ip->a0;
    const auto src_reg = c->ip->a1;

    context_set_upvalue(c, dest_uv_id, c->stack_bp[src_reg]);
    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_close_upvals(context* c) {
    const auto callee_closure_id = ((context_frame*) dynbuf_last_mut(&c->frames))->closure_id;

    if (callee_closure_id == -1) {
        c->flags = nl_ctxf_err_op;

        return;
    }

    context_close_upvalues(c, callee_closure_id);
    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_use_ic(context* c) {
    c->curr_ic_id = c->ip->a0;
    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_inst_trait(context* c) {
    const auto dest_reg = c->ip->a0;
    const auto trait_var_reg = c->ip->a1;
    const auto named_props_n = c->ip->a2;

    auto temp_object_handle = context_create_with_trait(c, c->stack_bp[trait_var_reg], named_props_n);

    if (temp_object_handle.id == -1) {
        c->flags = nl_ctxf_err_ref;

        return;
    }

    c->stack_bp[dest_reg] = (value) { .n = temp_object_handle.id, .tag = nl_vtag_obj };
    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_seal_obj(context* c) {
    const auto target_object_reg = c->ip->a0;

    context_seal_object(c, c->stack_bp[target_object_reg]);
    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_init_prop(context* c) {
    const auto dest_object_v = c->stack_bp[c->ip->a0];
    const auto dest_key_id = c->ip->a1;
    const auto src_v = c->stack_bp[c->ip->a2];

    context_init_property(c, dest_object_v, dest_key_id, src_v);
    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_set_prop(context* c) {
    const auto dest_object_v = c->stack_bp[c->ip->a0];
    const auto dest_key_id = c->ip->a1;
    const auto src_v = c->stack_bp[c->ip->a2];

    if (!context_set_property(c, dest_object_v, dest_key_id, src_v)) {
        c->flags = nl_ctxf_err_ref;

        return;
    }

    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_get_prop(context* c) {
    const auto dest_reg = c->ip->a0;
    const auto src_object_v = c->stack_bp[c->ip->a1];
    const auto src_key_id = c->ip->a2;

    auto prop_ref = context_get_property(c, src_object_v, src_key_id);

    if (!prop_ref) {
        c->flags = nl_ctxf_err_ref;

        return;
    } else {
        c->stack_bp[dest_reg] = *prop_ref;
        c->ip++;
    }

    TAILCALL
    return dispatch_op(c);
}

void op_set_item(context* c) {
    c->flags = nl_ctxf_err_op; // todo: implement

    return;
}

void op_get_item(context* c) {
    c->flags = nl_ctxf_err_op; // todo: implement

    return;
}

void op_ref_self(context* c) {
    const auto self_dest_reg = c->ip->a0;

    c->stack_bp[self_dest_reg] = (value) {
        .n = ((const context_frame*) dynbuf_last_unchecked(&c->frames))->self_obj_id,
        .tag = nl_vtag_obj
    };
    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_ld_accr(context* c) {
    const auto dest_reg = c->ip->a0;

    c->stack_bp[dest_reg] = c->accr;
    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_test_nil(context* c) {
    const auto src_and_dest_reg = c->ip->a0;
    const bool test_flag = c->stack_bp[src_and_dest_reg].tag == nl_vtag_nil;

    c->stack_bp[src_and_dest_reg] = (value) {
        .b = test_flag,
        .tag = nl_vtag_bool
    };
    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_neg_num(context* c) {
    const auto src_and_dest_reg = c->ip->a0;
    const auto target_value = c->stack_bp[src_and_dest_reg];
    float temp_f;

    switch (target_value.tag) {
        case nl_vtag_nil:
        case nl_vtag_bool: temp_f = NAN; break;
        case nl_vtag_int: temp_f = (float) -target_value.n; break;
        case nl_vtag_num: temp_f = -target_value.f; break;
        case nl_vtag_str: case nl_vtag_obj: default: temp_f = NAN; break;
    }

    c->stack_bp[src_and_dest_reg] = (value) {
        .f = temp_f,
        .tag = nl_vtag_num
    };
    c->ip++;
}

void op_neg_log(context* c) {
    const auto src_and_dest_reg = c->ip->a0;
    const auto target_value = c->stack_bp[src_and_dest_reg];

   c->stack_bp[src_and_dest_reg] = (value) {
        .b = !context_convert_to_bool(c, target_value),
        .tag = nl_vtag_bool
    };
    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_mod(context* c) {
    const auto dest_reg = c->ip->a0;
    const auto a1_reg = c->ip->a1;
    const auto a2_reg = c->ip->a2;
    const float a1_f32 = context_convert_to_f32(c, c->stack_bp[a1_reg]), a2_f32 = context_convert_to_f32(c, c->stack_bp[a2_reg]);

    c->stack_bp[dest_reg] = (value) {
        .f = fmodf(a1_f32, a2_f32),
        .tag = nl_vtag_num
    };
    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_mul(context* c) {
    const auto dest_reg = c->ip->a0;
    const auto a1_reg = c->ip->a1;
    const auto a2_reg = c->ip->a2;
    const float a1_f32 = context_convert_to_f32(c, c->stack_bp[a1_reg]), a2_f32 = context_convert_to_f32(c, c->stack_bp[a2_reg]);

    c->stack_bp[dest_reg] = (value) {
        .f = a1_f32 * a2_f32,
        .tag = nl_vtag_num
    };
    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_div(context* c) {
    const auto dest_reg = c->ip->a0;
    const auto a1_reg = c->ip->a1;
    const auto a2_reg = c->ip->a2;
    const float a1_f32 = context_convert_to_f32(c, c->stack_bp[a1_reg]), a2_f32 = context_convert_to_f32(c, c->stack_bp[a2_reg]);
    float result;

    if (isnan(a1_f32) || isnan(a2_f32) || a1_f32 + a2_f32 == 0.0f) {
        result = NAN;
    } else if (a2_f32 == 0.0f) {
        if (a1_f32 > 0.0f) {
            result = INFINITY;
        } else {
            result = -INFINITY;
        }
    } else {
        result = a1_f32 / a2_f32;
    }

    c->stack_bp[dest_reg] = (value) {
        .f = result,
        .tag = nl_vtag_num
    };
    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_add(context* c) {
    const auto dest_reg = c->ip->a0;
    const auto a1_reg = c->ip->a1;
    const auto a2_reg = c->ip->a2;

    c->stack_bp[dest_reg] = (value) {
        .f = context_convert_to_f32(c, c->stack_bp[a1_reg])
            + context_convert_to_f32(c, c->stack_bp[a2_reg]),
        .tag = nl_vtag_num
    };
    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_sub(context* c) {
    const auto dest_reg = c->ip->a0;
    const auto a1_reg = c->ip->a1;
    const auto a2_reg = c->ip->a2;

    c->stack_bp[dest_reg] = (value) {
        .f = context_convert_to_f32(c, c->stack_bp[a1_reg])
            - context_convert_to_f32(c, c->stack_bp[a2_reg]),
        .tag = nl_vtag_num
    };
    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_lt(context* c) {
    const auto dest_reg = c->ip->a0;
    const auto a1_reg = c->ip->a1;
    const auto a2_reg = c->ip->a2;

    c->stack_bp[dest_reg] = (value) {
        .b = context_convert_to_f32(c, c->stack_bp[a1_reg])
            < context_convert_to_f32(c, c->stack_bp[a2_reg]),
        .tag = nl_vtag_bool
    };
    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_lte(context* c) {
    const auto dest_reg = c->ip->a0;
    const auto a1_reg = c->ip->a1;
    const auto a2_reg = c->ip->a2;

    c->stack_bp[dest_reg] = (value) {
        .b = context_convert_to_f32(c, c->stack_bp[a1_reg])
            <= context_convert_to_f32(c, c->stack_bp[a2_reg]),
        .tag = nl_vtag_bool
    };
    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_gt(context* c) {
    const auto dest_reg = c->ip->a0;
    const auto a1_reg = c->ip->a1;
    const auto a2_reg = c->ip->a2;

    c->stack_bp[dest_reg] = (value) {
        .b = context_convert_to_f32(c, c->stack_bp[a1_reg])
            > context_convert_to_f32(c, c->stack_bp[a2_reg]),
        .tag = nl_vtag_bool
    };
    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_gte(context* c) {
    const auto dest_reg = c->ip->a0;
    const auto a1_reg = c->ip->a1;
    const auto a2_reg = c->ip->a2;

    c->stack_bp[dest_reg] = (value) {
        .b = context_convert_to_f32(c, c->stack_bp[a1_reg])
            >= context_convert_to_f32(c, c->stack_bp[a2_reg]),
        .tag = nl_vtag_bool
    };
    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_eq(context* c) {
    const auto dest_reg = c->ip->a0;
    const auto a1_reg = c->ip->a1;
    const auto a2_reg = c->ip->a2;
    const uint8_t boolean_flag = context_test_equality(c, c->stack_bp[a1_reg], c->stack_bp[a2_reg]) ? 0x1 : 0x0;

    c->stack_bp[dest_reg] = (value) {
        .b = boolean_flag,
        .tag = nl_vtag_bool
    };
    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_ne(context* c) {
    const auto dest_reg = c->ip->a0;
    const auto a1_reg = c->ip->a1;
    const auto a2_reg = c->ip->a2;

    // ? NOTE: `!=` operator is like NOT(LHS == RHS), so reuse of the equality method is ok.
    const uint8_t boolean_flag = context_test_equality(c, c->stack_bp[a1_reg], c->stack_bp[a2_reg]) ? 0x0 : 0x1;

    c->stack_bp[dest_reg] = (value) {
        .b = boolean_flag,
        .tag = nl_vtag_bool
    };
    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_jmp_not_nil(context* c) {
    const value* const src_value = c->stack_bp + c->ip->a0;
    const auto pc_offset = c->ip->a1;

    if (src_value->tag == nl_vtag_nil) {
        c->ip++;
    } else {
        c->ip = c->base_ip + pc_offset;
    }

    TAILCALL
    return dispatch_op(c);
}

void op_jmp_if(context* c) {
    const auto src_value = c->stack_bp[c->ip->a0];
    const auto pc_offset = c->ip->a1;

    if (context_convert_to_bool(c, src_value)) {
        c->ip = c->base_ip + pc_offset;
    } else {
        c->ip++;
    }

    TAILCALL
    return dispatch_op(c);
}

void op_jmp_else(context* c) {
    const auto src_value = c->stack_bp[c->ip->a0];
    const auto pc_offset = c->ip->a1;

    if (context_convert_to_bool(c, src_value)) {
        c->ip++;
    } else {
        c->ip = c->base_ip + pc_offset;
    }

    TAILCALL
    return dispatch_op(c);
}

void op_jmp(context* c) {
    const auto pc_offset = c->ip->a0;

    c->ip = c->base_ip + pc_offset;

    TAILCALL
    return dispatch_op(c);
}

void op_mk_closure(context* c) {
    const auto dest_reg = c->ip->a0;
    const auto callee_cid = c->ip->a1;

    c->stack_bp[dest_reg] = context_try_closurify(c, callee_cid);
    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_call(context* c) {
    const auto callee_v = c->stack_bp[c->ip->a0];

    // ! IMPORTANT: Continue at this caller after the callee runs (+ 2 to skip the operand data).
    if (!context_try_invoke(c, callee_v, -1)) {
        c->flags = nl_ctxf_err_ref;

        return;
    }

    TAILCALL
    return dispatch_op(c);
}

void op_call_n(context* c) {
    const auto n_fun = c->natives[c->ip->a0];
    const uint8_t n_argc = c->ip->a1; // ! UH OH: decide if narrowing the u16 to a u8 is ok for now... this may cause bugs later!

    if (!n_fun(c, n_argc)) {
        c->flags = nl_ctxf_err_op;

        return;
    }

    c->ip++;

    TAILCALL
    return dispatch_op(c);
}

void op_call_method(context* c) {
    const auto self_object_reg = c->stack_bp[c->ip->a0];
    const auto self_prop_id = c->ip->a1;

    if (!context_try_invoke_method(c, self_object_reg, self_prop_id)) {
        c->flags = nl_ctxf_err_op;

        return;
    }

    TAILCALL
    return dispatch_op(c);
}

void op_ret(context* c) {
    const context_frame* callee_frame = dynbuf_last_unchecked(&c->frames);
    const value result_v = c->stack_bp[c->ip->a0];

    c->accr = result_v; // ! IMPORTANT: yield temp result value in accumulator, later read after return to caller.
    c->base_ip = callee_frame->caller_base_ip;
    c->ip = callee_frame->caller_ip;
    c->kp = callee_frame->caller_kp;
    c->icp = callee_frame->caller_icaches;
    c->stack_bp = callee_frame->caller_stack_base;

    dynbuf_pop(&c->frames);

    if (dynbuf_size(&c->frames) == 0) {
        if (c->flags & nl_ctxf_pending) {
            c->flags = nl_ctxf_ok;
        }

        return;
    }

    TAILCALL
    return dispatch_op(c);
}



[[nodiscard]]
uint16_t nl_execute_with_context(context* c) {
    dispatch_op(c);

    return c->flags;
}
