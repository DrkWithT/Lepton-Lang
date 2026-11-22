#include "bytecode.h"



static constexpr size_t bc_unary_stride_v = 3;
static constexpr size_t bc_binary_stride_v = 5;
static constexpr size_t bc_ternary_stride_v = 7;

static const char* const bc_opcode_names_v[] = {
    "nop",
    "store_nil",
    "store_bool",
    "swap_r",
    "loadi_u16",
    "mov_rk",
    "mov_rr",
    "mov_rg",
    "sto_arg",
    "ld_arg",
    "ld_upval",
    "sto_upval",
    "nl_op_close_upvals",
    "ld_accr",
    "test_nil",
    "neg_num",
    "neg_log",
    "mod",
    "mul",
    "div",
    "add",
    "sub",
    "lt",
    "lte",
    "gt",
    "gte",
    "eq",
    "ne",
    "jmp_not_nil",
    "jmp_if",
    "jmp_else",
    "jmp",
    "mk_closure",
    "call",
    "call_n",
    "ret",
};

void chunk_del(void *c) {
    auto chunk_self = (chunk*)c;

    if (chunk_self->icv != nullptr) {
        free(chunk_self->icv);
        chunk_self->icv = nullptr;
    }

    if (chunk_self->kd != nullptr) {
        free(chunk_self->kd);
        chunk_self->kd = nullptr;
    }

    if (chunk_self->bc != nullptr) {
        free(chunk_self->bc);
        chunk_self->bc = nullptr;
    }
}

[[nodiscard]]
program program_dud() {
    return (program) {
        .pool_s = {
            .mem = dynbuf_dud(),
            .free_ids = dynbuf_dud(),
            .deleter = nullptr,
            .obj_size = 0,
            .cost = 0,
            .gc_cost = 0,
            .next_id = 0,
            .max_id = 0,
            .tenure_id = 0
        },
        .pool_o = {
            .mem = dynbuf_dud(),
            .free_ids = dynbuf_dud(),
            .deleter = nullptr,
            .obj_size = 0,
            .cost = 0,
            .gc_cost = 0,
            .next_id = 0,
            .max_id = 0,
            .tenure_id = 0
        },
        .pool_c = {
            .mem = dynbuf_dud(),
            .free_ids = dynbuf_dud(),
            .deleter = nullptr,
            .obj_size = 0,
            .cost = 0,
            .gc_cost = 0,
            .next_id = 0,
            .max_id = 0,
            .tenure_id = 0
        },
        .chunks = dynbuf_dud(),
        .gv = nullptr,
        .natives = nullptr,
        .gv_cap = 0,
        .gv_len = 0,
        .natives_cap = 0,
        .natives_len = 0,
        .entry_cid = 0
    };
}

void program_del(void* pg) {
    auto program_self = (program*)pg;

    ent_pool_del(&program_self->pool_c);
    ent_pool_del(&program_self->pool_o);
    ent_pool_del(&program_self->pool_s);

    if (program_self->gv) {
        free(program_self->gv);
        program_self->gv = nullptr;
    }

    if (program_self->natives) {
        free(program_self->natives);
        program_self->natives = nullptr;
    }

    dynbuf_del(&program_self->chunks);
}

void value_output(FILE* os, value v) {
    switch (v.tag) {
        case nl_vtag_nil: fprintf(os, "nil"); break;
        case nl_vtag_bool: fprintf(os, "bool(%c)", v.b ? 'T' : 'F'); break;
        case nl_vtag_int: fprintf(os, "i32(%d)", v.n); break;
        case nl_vtag_num: fprintf(os, "f32(%f)", v.f); break;
        case nl_vtag_str: fprintf(os, "str(id %d)", v.n); break;
        case nl_vtag_obj: fprintf(os, "obj(id %d)", v.n); break;
        case nl_vtag_closure: fprintf(os, "clos(id %d)", v.n); break;
        default: fprintf(os, "unknown(...)"); break;
    }
}

void inst_output(FILE* os, const uint8_t* ip, uint8_t arity) {
    fprintf(os, "%s ", bc_opcode_names_v[*ip]);

    switch (arity) {
        case 1:
            fprintf(os, "%u", ip[1] + (ip[2] << 8));
            break;
        case 2:
            fprintf(os, "%u, ", ip[1] + (ip[2] << 8));
            fprintf(os, "%u", ip[3] + (ip[4] << 8));
            break;
        case 3:
            fprintf(os, "%u, ", ip[1] + (ip[2] << 8));
            fprintf(os, "%u, ", ip[3] + (ip[4] << 8));
            fprintf(os, "%u", ip[5] + (ip[6] << 8));
            break;
        default:
            break;
    }
}

void chunk_output(FILE* os, const chunk* c) {
    const value* chunk_consts = c->kd;
    const auto chunk_consts_len = c->k_len;
    const uint8_t* chunk_bc = c->bc;
    const auto chunk_bc_len = c->code_len;

    fprintf(os, "\n---- STATS ----\n\nregister-window: %u\n", c->regs_n);

    fprintf(os, "\n---- CONSTANTS ----\n");

    for (uint16_t k_id = 0; k_id < chunk_consts_len; k_id++) {
        fprintf(os, "K_%u: ", k_id);
        value_output(os, chunk_consts[k_id]);
        fprintf(os, "\n");
    }

    fprintf(os, "\n---- CODE ----\n");

    for (uint16_t bc_byte_pos = 0; bc_byte_pos < chunk_bc_len; ) {
        fprintf(os, "%u: ", bc_byte_pos);

        switch ((op) chunk_bc[bc_byte_pos]) {
            case nl_op_nop: inst_output(os, chunk_bc + bc_byte_pos, 0); bc_byte_pos++; break;
            case nl_op_store_nil: inst_output(os, chunk_bc + bc_byte_pos, 1); bc_byte_pos += bc_unary_stride_v; break;
            case nl_op_store_bool: inst_output(os, chunk_bc + bc_byte_pos, 2); bc_byte_pos += bc_binary_stride_v; break;
            case nl_op_swap_r: inst_output(os, chunk_bc + bc_byte_pos, 2); bc_byte_pos += bc_binary_stride_v; break;
            case nl_op_loadi_u16: inst_output(os, chunk_bc + bc_byte_pos, 2); bc_byte_pos += bc_binary_stride_v; break;
            case nl_op_mov_rk: inst_output(os, chunk_bc + bc_byte_pos, 2); bc_byte_pos += bc_binary_stride_v; break;
            case nl_op_mov_rr: inst_output(os, chunk_bc + bc_byte_pos, 2); bc_byte_pos += bc_binary_stride_v; break;
            case nl_op_mov_rg: inst_output(os, chunk_bc + bc_byte_pos, 2); bc_byte_pos += bc_binary_stride_v; break;
            case nl_op_sto_arg: inst_output(os, chunk_bc + bc_byte_pos, 2); bc_byte_pos += bc_binary_stride_v; break;
            case nl_op_ld_arg: inst_output(os, chunk_bc + bc_byte_pos, 2); bc_byte_pos += bc_binary_stride_v; break;
            case nl_op_ld_upval: inst_output(os, chunk_bc + bc_byte_pos, 3); bc_byte_pos += bc_ternary_stride_v; break;
            case nl_op_sto_upval: inst_output(os, chunk_bc + bc_byte_pos, 3); bc_byte_pos += bc_ternary_stride_v; break;
            case nl_op_close_upvals: inst_output(os, chunk_bc + bc_byte_pos, 0); bc_byte_pos++; break;
            case nl_op_ld_accr: inst_output(os, chunk_bc + bc_byte_pos, 1); bc_byte_pos += bc_unary_stride_v; break;
            case nl_op_test_nil: inst_output(os, chunk_bc + bc_byte_pos, 1); bc_byte_pos += bc_unary_stride_v; break;
            case nl_op_neg_num: inst_output(os, chunk_bc + bc_byte_pos, 1); bc_byte_pos += bc_unary_stride_v; break;
            case nl_op_neg_log: inst_output(os, chunk_bc + bc_byte_pos, 1); bc_byte_pos += bc_unary_stride_v; break;
            case nl_op_mod: inst_output(os, chunk_bc + bc_byte_pos, 3); bc_byte_pos += bc_ternary_stride_v; break;
            case nl_op_mul: inst_output(os, chunk_bc + bc_byte_pos, 3); bc_byte_pos += bc_ternary_stride_v; break;
            case nl_op_div: inst_output(os, chunk_bc + bc_byte_pos, 3); bc_byte_pos += bc_ternary_stride_v; break;
            case nl_op_add: inst_output(os, chunk_bc + bc_byte_pos, 3); bc_byte_pos += bc_ternary_stride_v; break;
            case nl_op_sub: inst_output(os, chunk_bc + bc_byte_pos, 3); bc_byte_pos += bc_ternary_stride_v; break;
            case nl_op_lt: inst_output(os, chunk_bc + bc_byte_pos, 3); bc_byte_pos += bc_ternary_stride_v; break;
            case nl_op_lte: inst_output(os, chunk_bc + bc_byte_pos, 3); bc_byte_pos += bc_ternary_stride_v; break;
            case nl_op_gt: inst_output(os, chunk_bc + bc_byte_pos, 3); bc_byte_pos += bc_ternary_stride_v; break;
            case nl_op_gte: inst_output(os, chunk_bc + bc_byte_pos, 3); bc_byte_pos += bc_ternary_stride_v; break;
            case nl_op_eq: inst_output(os, chunk_bc + bc_byte_pos, 3); bc_byte_pos += bc_ternary_stride_v; break;
            case nl_op_ne: inst_output(os, chunk_bc + bc_byte_pos, 3); bc_byte_pos += bc_ternary_stride_v; break;
            case nl_op_jmp_not_nil: inst_output(os, chunk_bc + bc_byte_pos, 2); bc_byte_pos += bc_binary_stride_v; break;
            case nl_op_jmp_if: inst_output(os, chunk_bc + bc_byte_pos, 2); bc_byte_pos += bc_binary_stride_v; break;
            case nl_op_jmp_else: inst_output(os, chunk_bc + bc_byte_pos, 2); bc_byte_pos += bc_binary_stride_v; break;
            case nl_op_jmp: inst_output(os, chunk_bc + bc_byte_pos, 1); bc_byte_pos += bc_unary_stride_v; break;
            case nl_op_mk_closure: inst_output(os, chunk_bc + bc_byte_pos, 2); bc_byte_pos += bc_binary_stride_v; break;
            case nl_op_call: inst_output(os, chunk_bc + bc_byte_pos, 2); bc_byte_pos += bc_binary_stride_v; break;
            case nl_op_call_n: inst_output(os, chunk_bc + bc_byte_pos, 2); bc_byte_pos += bc_binary_stride_v; break;
            case nl_op_ret: inst_output(os, chunk_bc + bc_byte_pos, 1); bc_byte_pos += bc_unary_stride_v; break;
            default: fprintf(os, "(unknown op)"); return;
        }

        fprintf(os, "\n");
    }
}

void program_output(FILE* os, const program* pg) {
    const auto pg_chunks = (const chunk*) pg->chunks.data;
    const auto pg_globals = pg->gv;
    const uint16_t pg_chunks_n = pg->chunks.n;
    const auto pg_globals_n = pg->gv_len;
    const auto pg_entry_cid = pg->entry_cid;

    fprintf(os, "\x1b[1;33m--- PROGRAM DUMP ---\x1b[0m\n\n---- GLOBALS ----\n\n");

    for (uint16_t gv_id = 0; gv_id < pg_globals_n; gv_id++) {
        fprintf(os, "G_%u: ", gv_id);
        value_output(os, pg_globals[gv_id]);
        fprintf(os, "\n");
    }

    fprintf(os, "\n---- CHUNKS ----\n");

    for (uint16_t cid = 0; cid < pg_chunks_n; cid++) {
        fprintf(os, "\n---- CHUNK \x1b[1;36m%u\x1b[0m ----\n", cid);
        chunk_output(os, pg_chunks + cid);
    }
}
