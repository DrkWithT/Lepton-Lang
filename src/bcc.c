#include <stdio.h>
#include "bcc.h"
#include "closures.h"



static constexpr size_t bcc_initial_chunk_cap_v = 24;
static constexpr size_t bcc_initial_chunk_k_cap_v = 8;
static constexpr size_t bcc_initial_chunk_ic_cap_v = 2;
static constexpr size_t bcc_initial_chunk_var_ent_cap_v = 2;
static constexpr size_t bcc_bc_byte_sz = sizeof(uint8_t);
static constexpr uint16_t bcc_bc_max_cap_v = 0xffff;
static constexpr uint16_t bcc_initial_global_cap_v = 8;
static constexpr uint16_t bcc_initial_natives_cap_v = 8;
static constexpr uint16_t bcc_max_arity = 255;
static constexpr uint16_t bcc_max_errors = 3;

static const ic bcc_idle_ic_v = {
    .shape_ids = {[3] = -1},
    .recv_ids = {[3] = -1},
    .obj_offsets = {[3] = -1},
    .leeway = 1,            // ? idle ICs must gather data upon their initial miss.
    .state = nl_ics_idle,
};



[[nodiscard]]
bool bcc_flag_check_n(uint16_t flags, const uint16_t* checks, uint8_t n) {
    for (uint8_t chk_i = 0; chk_i < n; chk_i++) {
        if ((flags & checks[chk_i]) != 0) {
            return true;
        }
    }

    return false;
}

[[nodiscard]]
uint16_t bcc_flag_set_n(uint16_t flags, const uint16_t* sets, uint8_t n) {
    for (uint8_t i = 0; i < n; i++) {
        flags |= sets[i];
    }

    return flags;
}

[[nodiscard]]
uint16_t bcc_flag_unset_n(uint16_t flags, const uint16_t* omits, uint8_t n) {
    for (uint8_t i = 0; i < n; i++) {
        flags &= ~omits[i];
    }

    return flags;
}

static const auto bcc_dud_result_v = (bcc_result) {.n = 0, .flags = 0x00, .cid = 0x00, .domain = nl_bcc_sym_invalid, .with_extra = false };



typedef bcc_result(*bcc_emitter_t)(bcc_state* bcc, const ast_node* node, uint16_t flags);

[[nodiscard]]
bcc_result bcc_emit_invalid(bcc_state* bcc, const ast_node* node, uint16_t flags);
[[nodiscard]]
bcc_result bcc_emit_literal(bcc_state* bcc, const ast_node* node, uint16_t flags);
[[nodiscard]]
bcc_result bcc_emit_literal_array(bcc_state* bcc, const ast_node* node, uint16_t flags);
[[nodiscard]]
bcc_result bcc_emit_expr_new_table(bcc_state* bcc, const ast_node* node, uint16_t flags);
[[nodiscard]]
bcc_result bcc_emit_expr_func(bcc_state* bcc, const ast_node* node, uint16_t flags);
[[nodiscard]]
bcc_result bcc_emit_expr_access(bcc_state* bcc, const ast_node* node, uint16_t flags);
[[nodiscard]]
bcc_result bcc_emit_expr_call(bcc_state* bcc, const ast_node* node, uint16_t flags);
[[nodiscard]]
bcc_result bcc_emit_unary(bcc_state* bcc, const ast_node* node, uint16_t flags);
[[nodiscard]]
bcc_result bcc_emit_binary_normal(bcc_state* bcc, const ast_node* node, uint16_t flags);
[[nodiscard]]
bcc_result bcc_emit_binary_logical(bcc_state* bcc, const ast_node* node, uint16_t flags);
[[nodiscard]]
bcc_result bcc_emit_expr_assign(bcc_state* bcc, const ast_node* node, uint16_t flags);
[[nodiscard]]
bcc_result bcc_emit_cond(bcc_state* bcc, const ast_node* node, uint16_t flags);
[[nodiscard]]
bcc_result bcc_emit_loop(bcc_state* bcc, const ast_node* node, uint16_t flags);
[[nodiscard]]
bcc_result bcc_emit_loop_abator(bcc_state* bcc, const ast_node* node, uint16_t flags);
[[nodiscard]]
bcc_result bcc_emit_stmt_def(bcc_state* bcc, const ast_node* node, uint16_t flags);
[[nodiscard]]
bcc_result bcc_emit_block(bcc_state* bcc, const ast_node* node, uint16_t flags);
[[nodiscard]]
bcc_result bcc_emit_stmt_ret(bcc_state* bcc, const ast_node* node, uint16_t flags);
[[nodiscard]]
bcc_result bcc_emit_expr_stmt(bcc_state* bcc, const ast_node* node, uint16_t flags);

const bcc_emitter_t bcc_emitters[] = {
    bcc_emit_invalid,
    bcc_emit_literal,
    bcc_emit_literal_array,
    bcc_emit_expr_new_table,
    bcc_emit_expr_func,
    bcc_emit_expr_access,
    bcc_emit_expr_call,
    bcc_emit_unary,
    bcc_emit_binary_normal,
    bcc_emit_binary_logical,
    bcc_emit_expr_assign,
    bcc_emit_cond,
    bcc_emit_loop,
    bcc_emit_loop_abator,
    bcc_emit_stmt_def,
    bcc_emit_block,
    bcc_emit_stmt_ret,
    bcc_emit_expr_stmt,
};

NL_CXX_INLINE bcc_result bcc_emit_next(bcc_state* bcc, const ast_node* node, uint16_t flags) {
    return bcc_emitters[node->tag](bcc, node, flags);
}

void symscope_del(void* ssc) {
    auto symscope_self = (symscope*)ssc;

    dynbuf_del(&symscope_self->infos);
}

void bcc_loopinfo_del(void* lp_info) {
    auto loop_self = (bcc_loopinfo*) lp_info;

    dynbuf_del(&loop_self->exit_iposv);
    dynbuf_del(&loop_self->rep_iposv);
}


// ! IMPORTANT: This returns the parent chunk ID before entering this new child chunk.
[[nodiscard]]
uint16_t bcc_start_chunk(bcc_state* bcc) {
    const uint16_t next_cid = bcc->chunks.n;

    chunk next_chunk = {
        .bc = calloc(bcc_initial_chunk_cap_v, bcc_bc_byte_sz),
        .code_cap = bcc_initial_chunk_cap_v,
        .code_len = 0,
        .kd = calloc(bcc_initial_chunk_k_cap_v, sizeof(value)),
        .k_cap = bcc_initial_chunk_k_cap_v,
        .k_len = 0,
        .icv = calloc(bcc_initial_chunk_ic_cap_v, sizeof(ic)),
        .ic_cap = bcc_initial_chunk_ic_cap_v,
        .ic_len = 1,
        .regs_n = 0,
        .arity = 0,
    };

    next_chunk.icv[0] = (ic) {
        .shape_ids = {[3] = -1},
        .recv_ids = {[3] = -1},
        .obj_offsets = {[3] = -1},
        .leeway = 0,
        .state = nl_ics_dead
    };

    dynbuf_push(&bcc->chunks, &next_chunk);

    return next_cid;
}

NL_CXX_INLINE void bcc_resume_chunk(bcc_state* bcc, uint16_t cid) {
    bcc->curr_cid = cid;
}

[[nodiscard]]
NL_CXX_INLINE uint16_t bcc_next_ip(const bcc_state* bcc) {
    const auto curr_chunk = (const chunk*) dynbuf_get(&bcc->chunks, bcc->curr_cid);

    return curr_chunk->code_len;
}

void bcc_emit_byte(bcc_state* bcc, uint8_t b) {
    auto curr_chunk = (chunk*) dynbuf_get_mut(&bcc->chunks, bcc->curr_cid);
    const uint16_t next_len = curr_chunk->code_len;
    const uint16_t curr_cap = curr_chunk->code_cap;
    const uint16_t next_cap = curr_chunk->code_cap * 2;

    if (next_len >= bcc_bc_max_cap_v) {
        NL_PANIC("Exhausted u16-max capacity of chunk in bcc here.", __FILE__, __LINE__)
    }

    if (next_len >= curr_cap) {
        auto next_bc_buf = realloc(curr_chunk->bc, bcc_bc_byte_sz * next_cap);

        if (!next_bc_buf) {
            NL_PANIC("Failed to reallocate bytecode buffer in bcc here.", __FILE__, __LINE__)
        }

        curr_chunk->bc = next_bc_buf;
        curr_chunk->code_cap = next_cap;
    }

    curr_chunk->bc[next_len] = b;
    curr_chunk->code_len++;
}

uint16_t bcc_emit_inst_nonary(bcc_state* bcc, op o) {
    const uint16_t bc_ipos = ((const chunk*)dynbuf_get(&bcc->chunks, bcc->curr_cid))->code_len;

    bcc_emit_byte(bcc, (uint8_t) o);

    return bc_ipos;
}

uint16_t bcc_emit_inst_unary(bcc_state* bcc, op o, uint16_t a0) {
    const uint16_t bc_ipos = ((const chunk*) dynbuf_get(&bcc->chunks, bcc->curr_cid))->code_len;

    bcc_emit_byte(bcc, (uint8_t) o);
    bcc_emit_byte(bcc, (uint8_t) (a0 & 0xff));
    bcc_emit_byte(bcc, (uint8_t) ((a0 & 0xff00) >> 8));

    return bc_ipos;
}

uint16_t bcc_emit_inst_binary(bcc_state* bcc, op o, uint16_t a0, uint16_t a1) {
    const uint16_t bc_ipos = ((const chunk*)dynbuf_get(&bcc->chunks, bcc->curr_cid))->code_len;

    bcc_emit_byte(bcc, (uint8_t) o);
    bcc_emit_byte(bcc, (uint8_t) (a0 & 0xff));
    bcc_emit_byte(bcc, (uint8_t) ((a0 & 0xff00) >> 8));
    bcc_emit_byte(bcc, (uint8_t) (a1 & 0xff));
    bcc_emit_byte(bcc, (uint8_t) ((a1 & 0xff00) >> 8));

    return bc_ipos;
}

uint16_t bcc_emit_inst_ternary(bcc_state* bcc, op o, uint16_t a0, uint16_t a1, uint16_t a2) {
    const uint16_t bc_ipos = ((const chunk*)dynbuf_get(&bcc->chunks, bcc->curr_cid))->code_len;

    bcc_emit_byte(bcc, (uint8_t) o);
    bcc_emit_byte(bcc, (uint8_t) (a0 & 0xff));
    bcc_emit_byte(bcc, (uint8_t) ((a0 & 0xff00) >> 8));
    bcc_emit_byte(bcc, (uint8_t) (a1 & 0xff));
    bcc_emit_byte(bcc, (uint8_t) ((a1 & 0xff00) >> 8));
    bcc_emit_byte(bcc, (uint8_t) (a2 & 0xff));
    bcc_emit_byte(bcc, (uint8_t) ((a2 & 0xff00) >> 8));

    return bc_ipos;
}

void bcc_patch_cond_jump(bcc_state* bcc, uint16_t target_ip, uint16_t dest_ipos) {
    auto curr_chunk = (chunk*) dynbuf_get_mut(&bcc->chunks, bcc->curr_cid);
    const op checked_opcode = curr_chunk->bc[target_ip];

    if (checked_opcode == nl_op_jmp_if || checked_opcode == nl_op_jmp_else) {
        // ? These offsets encode the u16 in operand `a1`: the absolute IP
        curr_chunk->bc[target_ip + 3] = (dest_ipos & 0xff);
        curr_chunk->bc[target_ip + 4] = (dest_ipos & 0xff00) >> 8;
    }
}

void bcc_patch_jump(bcc_state* bcc, uint16_t target_ip, uint16_t dest_ipos) {
    auto curr_chunk = (chunk*) dynbuf_get_mut(&bcc->chunks, bcc->curr_cid);
    const op checked_opcode = curr_chunk->bc[target_ip];

    if (checked_opcode == nl_op_jmp) {
        // ? These offsets encode the u16 in operand `a0`: the absolute IP
        curr_chunk->bc[target_ip + 1] = (dest_ipos & 0xff);
        curr_chunk->bc[target_ip + 2] = (dest_ipos & 0xff00) >> 8;
    }
}

[[nodiscard]]
uint16_t nl_bcc_place_ic(bcc_state* bcc, ic temp_ic) {
    auto curr_chunk = (chunk*) dynbuf_get_mut(&bcc->chunks, bcc->curr_cid);
    const uint16_t next_ic_len = curr_chunk->ic_len;
    const uint16_t curr_ic_cap = curr_chunk->ic_cap;
    const uint16_t next_ic_cap = curr_chunk->ic_cap * 2;

    if (next_ic_len >= bcc_bc_max_cap_v) {
        NL_PANIC("Exhausted u16-max capacity of chunk in bcc here.", __FILE__, __LINE__)
    }

    if (next_ic_len >= curr_ic_cap) {
        auto next_ic_buf = realloc(curr_chunk->icv, sizeof(ic) * next_ic_cap);

        if (!next_ic_buf) {
            NL_PANIC("Failed to reallocate bytecode buffer in bcc here.", __FILE__, __LINE__)
        }

        curr_chunk->icv = next_ic_buf;
        curr_chunk->code_cap = next_ic_cap;
    }

    curr_chunk->icv[next_ic_len] = temp_ic;
    curr_chunk->ic_len++;

    return next_ic_len;
}

void bcc_patch_upvals_n(bcc_state* bcc) {
    const symscope* curr_scope = dynbuf_last(&bcc->scopes);
    const syminfo* const curr_infos = curr_scope->infos.data;
    chunk* curr_chunk = dynbuf_get_mut(&bcc->chunks, bcc->curr_cid);
    const uint16_t curr_infos_n = curr_scope->infos.n;

    uint32_t* upval_metadata = calloc(4, sizeof(uint32_t));
    uint16_t upval_cap = 4;
    uint16_t upval_count = 0;

    for (uint16_t curr_info_pos = 0; curr_info_pos < curr_infos_n; curr_info_pos++) {
        if ((curr_infos + curr_info_pos)->tag == nl_bcc_sym_foreign_local) {
            const auto curr_upval_cid = (curr_infos + curr_info_pos)->cid;
            const uint16_t curr_upval_slot = (curr_infos + curr_info_pos)->n;

            if (upval_count >= upval_cap) {
                const uint16_t upval_cap_2 = (3 * upval_cap) / 2;
                uint32_t* upval_metadata_2 = realloc(upval_metadata, sizeof(uint32_t) * upval_cap_2);

                if (!upval_metadata_2) {
                    NL_PANIC("Failed to grow buffer of upvalue data here.", __FILE__, __LINE__)
                }

                upval_metadata = upval_metadata_2;
                upval_cap = upval_cap_2;
            }

            upval_metadata[upval_count] = curr_upval_cid | (curr_upval_slot << 16);
            upval_count++;
        }
    }

    curr_chunk->upval_entries = upval_metadata;
    curr_chunk->upvals_n = upval_count;
}

void bcc_start_scope(bcc_state* bcc, str scope_name) {
    symscope next_scope = {
        .infos = dynbuf_new(sizeof(syminfo), 8, nullptr),
        .name = scope_name,
        .top_konst = 0,
        .next_ic = 0,
        .cid = bcc->curr_cid
    };

    dynbuf_push(&bcc->scopes, &next_scope);
}

void bcc_end_scope(bcc_state* bcc) {
    dynbuf_pop(&bcc->scopes);
}

void bcc_start_loop(bcc_state* bcc, uint16_t loop_acc_reg) {
    bcc_loopinfo next_loop = {
        .rep_iposv = dynbuf_new(sizeof(uint16_t), 4, nullptr),
        .exit_iposv = dynbuf_new(sizeof(uint16_t), 4, nullptr),
        .acc_reg = loop_acc_reg
    };

    dynbuf_push(&bcc->loops, &next_loop);
}

void bcc_end_loop(bcc_state* bcc) {
    dynbuf_pop(&bcc->loops);
}

NL_CXX_INLINE void bcc_update_curr_rc(bcc_state* bcc, uint16_t next_rc) {
    auto curr_chunk = (chunk*) dynbuf_get_mut(&bcc->chunks, bcc->curr_cid);

    if (next_rc > curr_chunk->regs_n) {
        curr_chunk->regs_n = next_rc;
    }
}

[[nodiscard]]
int32_t bcc_normalize_to_local_reg(bcc_state* bcc, bcc_result r) {
    const auto r_tag = r.domain; // todo: add u16-max check for reserved `top_reg`!

    if (r_tag == nl_bcc_sym_local_r) {
        return r.n;
    } else if (r_tag == nl_bcc_sym_foreign_local) {
        const auto temp_local_r = bcc->top_reg++;

        bcc_update_curr_rc(bcc, bcc->top_reg);
        bcc_emit_inst_ternary(bcc, nl_op_ld_upval, temp_local_r, r.n, r.cid);

        return temp_local_r;
    }

    return -1;
}

NL_CXX_INLINE void bcc_report_err(bcc_state* bcc, const char* msg, uint16_t line, uint16_t col) {
    bcc->errors++;

    fprintf(
        stdout,
        "\x1b[1;31mCompile Error [%d]\x1b[0m ~ ln \x1b[1;36m%d\x1b[0m, col \x1b[1;36m%d\x1b[0m:\n\t%s\n",
        bcc->errors,
        line,
        col,
        msg
    );
}

// ! For symbol scope lookup mode: `G` is global-only, `L` is local-only, and `B` is bubbling.
[[nodiscard]]
const syminfo* bcc_lookup_symbol(bcc_state* bcc, str s, symtag domain, char mode) {
    int32_t scope_depth = (mode == 'G') ? 0 : bcc->scopes.n - 1;
    const auto should_bubble_lookup = mode == 'B';

    for (; scope_depth >= 0; scope_depth--) {
        const auto curr_scope = (const symscope*)dynbuf_get(&bcc->scopes, scope_depth);
        const auto scope_entries = (const syminfo*)dynbuf_data(&curr_scope->infos);
        const int32_t scope_entry_n = dynbuf_size(&curr_scope->infos);

        for (int32_t entry_pos = 0; entry_pos < scope_entry_n; entry_pos++) {
            const auto curr_entry = scope_entries + entry_pos;

            if (str_eq_str(&curr_entry->name, &s) && curr_entry->tag == domain) {
                return curr_entry;
            }
        }

        if (!should_bubble_lookup) {
            break;
        }
    }

    return nullptr;
}

[[nodiscard]]
const syminfo* bcc_push_symbol_to(bcc_state* bcc, syminfo info, char should_use_global) {
    auto target_scope = (symscope*) dynbuf_get_mut(&bcc->scopes, (should_use_global == 'G') ? 0 : bcc->scopes.n - 1 );

    return (const syminfo*) dynbuf_push(&target_scope->infos, &info);
}

[[nodiscard]]
uint16_t bcc_push_konst(bcc_state* bcc, value v) {
    auto curr_chunk = (chunk*) dynbuf_get_mut(&bcc->chunks, bcc->curr_cid);

    for (int32_t pre_k_id = 0; pre_k_id < curr_chunk->k_len; pre_k_id++) {
        if (!memcmp(curr_chunk->kd + pre_k_id, &v, sizeof(value))) return pre_k_id;
    }

    const auto next_k_id = curr_chunk->k_len;
    const auto curr_k_cap = curr_chunk->k_cap;
    const auto new_k_cap = curr_chunk->k_cap * 2;

    if (next_k_id >= curr_k_cap) {
        auto new_kd_buf = realloc(curr_chunk->kd, sizeof(value) * new_k_cap);

        if (!new_kd_buf) {
            NL_PANIC("Failed to grow constants buffer of chunk here.", __FILE__, __LINE__)
        }

        curr_chunk->kd = new_kd_buf;
        curr_chunk->k_cap = new_k_cap;
    }

    curr_chunk->kd[next_k_id] = v;
    curr_chunk->k_len++;

    return next_k_id;
}

[[nodiscard]]
uint16_t bcc_record_prop_key_imm(bcc_state* bcc, str prop_key) {
    const auto pre_found_info = bcc_lookup_symbol(bcc, prop_key, nl_bcc_sym_prop_key_imm, 'L');

    if (pre_found_info) {
        return pre_found_info->n;
    }

    symscope* curr_scope = dynbuf_last_mut(&bcc->scopes);
    const syminfo next_prop_key_info = {
        .name = prop_key,
        .n = curr_scope->next_key_id++,
        .cid = curr_scope->cid,
        .tag = nl_bcc_sym_prop_key_imm
    };

    return ((const syminfo*) dynbuf_push(&curr_scope->infos, &next_prop_key_info))->n;
}

[[nodiscard]]
const syminfo* bcc_record_string_k(bcc_state* bcc, str s) {
    const auto pre_found_info = bcc_lookup_symbol(bcc, (str) {.s = s.s - 1, .n = s.n + 2}, nl_bcc_sym_k, 'L'); // ! include quotes of string lexeme to distinguish string literals from naked words
    if (pre_found_info) {
        return pre_found_info;
    }

    // ? All string literals are global, so use the bottom-most, global scope context to track each.
    auto s_owned = string_from_str(&s);
    auto interned_sd = ent_pool_add(&bcc->pool_s, &s_owned);

    return bcc_push_symbol_to(bcc, (syminfo) {
        .name = (str) {.s = s.s - 1, .n = s.n + 2}, // ! include quotes of string lexeme to distinguish string literals from naked words
        .n = bcc_push_konst(bcc, (value) {
            .n = interned_sd.id,
            .tag = nl_vtag_str
        }),
        .cid = bcc->curr_cid,
        .tag = nl_bcc_sym_k
    }, 'L');
}

[[nodiscard]]
const syminfo* bcc_record_global(bcc_state* bcc, str s, value v) {
    const auto pre_found_info = bcc_lookup_symbol(bcc, s, nl_bcc_sym_global_k, 'G');
    if (pre_found_info) {
        return pre_found_info;
    }

    const auto next_global_v_id = bcc->gvals_n;
    const auto old_global_cap = bcc->gvals_cap;
    const auto new_global_cap = bcc->gvals_cap * 2;

    if (next_global_v_id >= old_global_cap) {
        auto temp_gval_buf = realloc(bcc->gvals, sizeof(value) * new_global_cap);

        if (!temp_gval_buf) {
            NL_PANIC("Failed to grow superglobal buffer in BCC here.", __FILE__, __LINE__)
        }

        bcc->gvals = temp_gval_buf;
        bcc->gvals_cap = new_global_cap;
    }

    bcc->gvals[next_global_v_id] = v;
    bcc->gvals_n++;

    return bcc_push_symbol_to(bcc, (syminfo) {
        .name = s,
        .n = next_global_v_id,
        .cid = 0,
        .tag = nl_bcc_sym_global_k
    }, 'G');
}

[[nodiscard]]
const syminfo* bcc_record_local(bcc_state* bcc, str name) {
    const auto pre_found_info = bcc_lookup_symbol(bcc, name, nl_bcc_sym_local_r, 'L');
    if (pre_found_info) {
        return pre_found_info;
    }

    auto curr_scope = (symscope*) dynbuf_last_mut(&bcc->scopes);

    const auto top_reg_for_local = bcc->top_reg;
    bcc->top_reg++;

    return bcc_push_symbol_to(bcc, (syminfo) {
        .name = name,
        .n = top_reg_for_local,
        .cid = bcc->curr_cid,
        .tag = nl_bcc_sym_local_r
    }, 'L');
}

void bcc_record_upval_info(bcc_state* bcc, const syminfo* uv_info) {
    auto curr_scope = (symscope*) dynbuf_last_mut(&bcc->scopes);
    const syminfo* info_it = curr_scope->infos.data;
    const syminfo* const info_end = info_it + curr_scope->infos.n;

    for (; info_it != info_end; info_it++) {
        if (info_it->tag == nl_bcc_sym_foreign_local && info_it->cid == uv_info->cid && info_it->n == uv_info->n) {
            return;
        }
    }

    return (void) bcc_push_symbol_to(bcc, *uv_info, 'L');
}

// ! IMPORTANT: upvalues are pre-recorded per chunk in vinfos as a var_ent for upvalue having `cid = assoc-scope -> assoc-function -> func-chunk-ID` and local-reg "offset"
// void bcc_save_chunk_var_ent(bcc_state* bcc, var_ent entry) {
//     auto curr_chunk = (chunk*) dynbuf_get_mut(&bcc->chunks, bcc->curr_cid);

//     const auto var_entries = (const var_ent*) curr_chunk->vinfo.data;
//     const uint16_t var_entry_n = curr_chunk->vinfo.n;

//     for (uint16_t var_entry_id = 0; var_entry_id < var_entry_n; var_entry_id++) {
//         const var_ent* const checked_var_entry = (var_entries + var_entry_id);

//         if (checked_var_entry->cid == entry.cid && checked_var_entry->id == entry.id) {
//             return;
//         }
//     }

//     dynbuf_push(&curr_chunk->vinfo, &entry);
// }


[[nodiscard]]
bcc_result bcc_emit_invalid(bcc_state* bcc, const ast_node* node, uint16_t flags) {
    return bcc_dud_result_v;
}

[[nodiscard]]
bcc_result bcc_preemit_identifier(bcc_state* bcc, const str* identifier, uint16_t flags) {
    const syminfo* found_named_info = nullptr;
    syminfo actual_info = {
        .name = str_dud(),
        .n = 0,
        .cid = 0,
        .tag = nl_bcc_sym_invalid
    };
    const auto ident_str = *identifier;

    if ((found_named_info = bcc_lookup_symbol(bcc, ident_str, nl_bcc_sym_global_k, 'G')) != nullptr) {
        if (!bcc_flag_check_n(flags, (uint16_t[]) { nl_bcc_cf_in_of_lhs }, 1)) {
            bcc_emit_inst_binary(bcc, nl_op_mov_rg, bcc->top_reg, found_named_info->n);
            actual_info.n = bcc->top_reg++;
            actual_info.cid = bcc->curr_cid;
            actual_info.tag = nl_bcc_sym_local_r;
        }
    } else if ((found_named_info = bcc_lookup_symbol(bcc, ident_str, nl_bcc_sym_local_r, 'L')) != nullptr) {
        if (!bcc_flag_check_n(flags, (uint16_t[]) { nl_bcc_cf_in_of_lhs }, 1)) {
            bcc_emit_inst_binary(bcc, nl_op_mov_rr, bcc->top_reg, found_named_info->n);
            actual_info.n = bcc->top_reg++;
            actual_info.cid = bcc->curr_cid;
            actual_info.tag = nl_bcc_sym_local_r;
        } else {
            actual_info = *found_named_info;
        }
    } else if ((found_named_info = bcc_lookup_symbol(bcc, ident_str, nl_bcc_sym_local_r, 'B')) != nullptr) {
        if (!bcc_flag_check_n(flags, (uint16_t[]) { nl_bcc_cf_in_of_lhs }, 1)) {
            bcc_emit_inst_ternary(bcc, nl_op_ld_upval, bcc->top_reg, found_named_info->n, found_named_info->cid);

            actual_info.n = bcc->top_reg++;
            actual_info.cid = bcc->curr_cid;
            actual_info.tag = nl_bcc_sym_local_r;

            if (found_named_info->cid != bcc->curr_cid) {
                const syminfo temp_upval_info = {
                    .name = found_named_info->name,
                    .n = found_named_info->n,
                    .cid = found_named_info->cid,
                    .tag = nl_bcc_sym_foreign_local
                };

                bcc_record_upval_info(bcc, &temp_upval_info);
            }
        } else if (found_named_info->cid == bcc->curr_cid) {
            actual_info.n = bcc->top_reg++;
            actual_info.cid = bcc->curr_cid;
            actual_info.tag = nl_bcc_sym_local_r;
        } else {
            actual_info.n = found_named_info->n;
            actual_info.cid = found_named_info->cid;
            actual_info.tag = nl_bcc_sym_foreign_local;

            bcc_record_upval_info(bcc, &actual_info);
        }
    }

    if (!found_named_info) {
        bcc_report_err(bcc, "Failed to resolve this name...", bcc->line, bcc->col);
        fprintf(stdout, "\tnote: was the name below defined in the function / outer scopes? '\x1b[1;31m");
        str_output(stdout, identifier);
        fprintf(stdout, "\x1b[0m'\n\n");

        return bcc_dud_result_v;
    } else if (actual_info.tag == nl_bcc_sym_invalid) {
        bcc_report_err(bcc, "Found a wrongly-resolved name. Also, globals are non-assignable constants.", bcc->line, bcc->col);
        fprintf(stdout, "\tnote: check definition of name '\x1b[1;31m");
        str_output(stdout, identifier);
        fprintf(stdout, "\x1b[0m'\n\n");

        return bcc_dud_result_v;
    }

    bcc_update_curr_rc(bcc, bcc->top_reg);

    // ! IMPORTANT: assume that only property-accessed and variable-name locations are assignable. This should be enforced via checking `ast_node::tag` for literal-ident or prop-access in `bcc_emit_assignment()`
    return (bcc_result) {
        .n = actual_info.n,
        .flags = flags,
        .cid = actual_info.cid,
        .domain = actual_info.tag,
        .with_extra = false 
    };
}

[[nodiscard]]
bcc_result bcc_emit_literal(bcc_state* bcc, const ast_node* node, uint16_t flags) {
    const auto literal_node = &node->data.lit;
    const auto temp_tk = &literal_node->token;
    const auto temp_lexeme = tk_get_lexeme(temp_tk, bcc->source);
    const auto temp_rvalue_flags = bcc_flag_unset_n(flags, (uint16_t[]) { nl_bcc_cf_in_of_lhs }, 1);

    bcc->line = temp_tk->line;
    bcc->col = temp_tk->col;

    switch (temp_tk->tag) {
        case nl_tk_kw_nil: {
            bcc_emit_inst_unary(bcc, nl_op_store_nil, bcc->top_reg);
            bcc_update_curr_rc(bcc, bcc->top_reg + 1);

            return (bcc_result) {
                .n = bcc->top_reg++,
                .flags = temp_rvalue_flags,
                .cid = 0,
                .domain = nl_bcc_sym_local_r,
                .with_extra = false
            };
        }
        case nl_tk_kw_true: case nl_tk_kw_false: {
            bcc_emit_inst_binary(bcc, nl_op_store_bool, bcc->top_reg, temp_tk->tag == nl_tk_kw_true);
            bcc_update_curr_rc(bcc, bcc->top_reg + 1);

            return (bcc_result) {
                .n = bcc->top_reg++,
                .flags = temp_rvalue_flags,
                .cid = 0,
                .domain = nl_bcc_sym_local_r,
                .with_extra = false
            };
        }
        case nl_tk_kw_self: {
            bcc_emit_inst_unary(bcc, nl_op_ref_self, bcc->top_reg);

            return (bcc_result) {
                .n = bcc->top_reg++,
                .flags = temp_rvalue_flags,
                .cid = bcc->curr_cid,
                .domain = nl_bcc_sym_local_r,
                .with_extra = false
            };
        }
        case nl_tk_identifier: {
            return bcc_preemit_identifier(bcc, &temp_lexeme, flags);
        }
        case nl_tk_lt_int: {
            const auto konst_i32_id = bcc_push_konst(bcc, (value) {
                .n = str_to_i32(&temp_lexeme),
                .tag = nl_vtag_int
            });

            bcc_emit_inst_binary(bcc, nl_op_mov_rk, bcc->top_reg, konst_i32_id);
            bcc_update_curr_rc(bcc, bcc->top_reg + 1);

            return (bcc_result) {
                .n = bcc->top_reg++,
                .flags = temp_rvalue_flags,
                .cid = 0,
                .domain = nl_bcc_sym_local_r,
                .with_extra = false
            };
        }
        case nl_tk_lt_num: {
            const auto k_f32_id = bcc_push_konst(bcc, (value) {
                .f = str_to_f32(&temp_lexeme),
                .tag = nl_vtag_num
            });

            bcc_emit_inst_binary(bcc, nl_op_mov_rk, bcc->top_reg, k_f32_id);
            bcc_update_curr_rc(bcc, bcc->top_reg + 1);

            return (bcc_result) {
                .n = bcc->top_reg++,
                .flags = temp_rvalue_flags,
                .cid = 0,
                .domain = nl_bcc_sym_local_r,
                .with_extra = false
            };
        }
        case nl_tk_lt_plain_str: {
            const auto plain_str_lit_info = bcc_record_string_k(bcc, temp_lexeme);

            bcc_emit_inst_binary(bcc, nl_op_mov_rk, bcc->top_reg, plain_str_lit_info->n);
            bcc_update_curr_rc(bcc, bcc->top_reg + 1);

            return (bcc_result) {
                .n = bcc->top_reg++,
                .flags = temp_rvalue_flags,
                .cid = 0,
                .domain = nl_bcc_sym_local_r,
                .with_extra = false
            };
        }
        case nl_tk_lt_esc_str:
        default: {
            bcc_report_err(bcc, "Unsupported literal found here: expected nil, boolean, name, number, or unescaped string.", bcc->line, bcc->col);
            return bcc_dud_result_v;
        }
    }
}

[[nodiscard]]
bcc_result bcc_emit_literal_array(bcc_state* bcc, const ast_node* node, uint16_t flags) {
    return bcc_dud_result_v; // todo: implement
}

[[nodiscard]]
bcc_result bcc_emit_expr_func(bcc_state* bcc, const ast_node* node, uint16_t flags) {
    const auto lambda_node = &node->data.fun;
    const auto lambda_params = (const tk*) lambda_node->param_tkv.data;
    const uint16_t lambda_arity = lambda_node->param_tkv.n;
    const auto lambda_body = lambda_node->body;
    const auto lambda_line = lambda_params->line, lambda_col = lambda_params->col;

    // ! Generate anonymous function name for user convenience if an error happens.
    auto internal_lambda_name = string_new("lambda-", 7);

    {
        char line_text[7] = {0};
        char col_text[7] = {0};
        const auto lambda_line_cc = snprintf(line_text, 7, "L%u", lambda_line);
        const auto lambda_col_cc = snprintf(col_text, 7, "C%u", lambda_col);

        string_append_raw(&internal_lambda_name, line_text, lambda_line_cc);
        string_append_raw(&internal_lambda_name, col_text, lambda_col_cc);
    }

    const auto internal_lambda_reg = bcc->top_reg++;
    const uint16_t prev_cid = bcc->curr_cid;

    // ! Begin symbol scope & code to track & generate respectively.
    const uint16_t lambda_cid = bcc_start_chunk(bcc);
    bcc->curr_cid = lambda_cid;
    bcc->top_reg = 0;

    bcc_start_scope(bcc, str_from_string(&internal_lambda_name));

    for (uint16_t param_pos = 0; param_pos < lambda_arity; param_pos++) {
        const auto param_name = tk_get_lexeme(lambda_params + param_pos, bcc->source);
        const auto param_local_reg = bcc_record_local(bcc, param_name)->n;

        bcc_emit_inst_binary(bcc, nl_op_ld_arg, param_local_reg, param_pos);
    }

    const auto body_flags = bcc_flag_set_n(bcc_flag_unset_n(flags, (uint16_t[]) { nl_bcc_cf_in_of_lhs }, 1), (uint16_t[]) { nl_bcc_cf_started_fun_body }, 1);
    const auto checked_body_result = bcc_emit_next(bcc, lambda_body, body_flags);

    if (!bcc_flag_check_n(checked_body_result.flags, (uint16_t[]) { nl_bcc_cf_ok }, 1)) {
        string_del(&internal_lambda_name);
        bcc_end_scope(bcc);
        bcc->curr_cid = prev_cid;
        bcc->top_reg = internal_lambda_reg;

        return checked_body_result;
    }

    // ! Put trailing `ret` in case the user forgets to put their result expression. The block emitter should always force its last (result) value into `acc` via `nl_op_ret`.
    bcc_emit_inst_nonary(bcc, nl_op_close_upvals);
    bcc_emit_inst_unary(bcc, nl_op_ret, checked_body_result.n);

    // ! Pre-count upvalues of chunk / function.
    bcc_patch_upvals_n(bcc);

    string_del(&internal_lambda_name);
    bcc_end_scope(bcc);
    bcc->curr_cid = prev_cid;
    bcc->top_reg = internal_lambda_reg;

    // ! Emit lambda as temporary into parent scope's register for later use.
    bcc_emit_inst_binary(bcc, nl_op_mk_closure, internal_lambda_reg, lambda_cid);
    bcc_update_curr_rc(bcc, bcc->top_reg);

    return (bcc_result) {
        .n = internal_lambda_reg,
        .flags = flags,
        .cid = bcc->curr_cid,
        .domain = nl_bcc_sym_local_r,
        .with_extra = false
    };
}

[[nodiscard]]
bcc_result bcc_emit_expr_new_table(bcc_state* bcc, const ast_node* node, uint16_t flags) {
    const auto new_table_node = &node->data.tbl;
    const tfield* new_table_entry_it = new_table_node->props.data;
    const tfield* const new_table_entry_end = new_table_node->props.data + new_table_node->props.n;
    const uint16_t new_table_trait_name_line = new_table_node->trait_name.line, new_table_trait_name_col = new_table_node->trait_name.col;

    const auto object_temp_reg = bcc->top_reg++;
    const auto trait_var_info = bcc_lookup_symbol(
        bcc,
        tk_get_lexeme(&new_table_node->trait_name, bcc->source),
        nl_bcc_sym_local_r,
        'B'
    );

    if (!trait_var_info) {
        bcc_report_err(bcc, "Expected valid trait name here, did you declare it?", new_table_trait_name_line, new_table_trait_name_col);

        return bcc_dud_result_v;
    }

    const uint16_t trait_var_reg = bcc_normalize_to_local_reg(bcc, (bcc_result) {
        .n = trait_var_info->n,
        .cid = trait_var_info->cid,
        .flags = flags,
        .domain = trait_var_info->tag,
        .with_extra = false
    });

    bcc_emit_inst_ternary(bcc, nl_op_inst_trait, object_temp_reg, trait_var_reg, new_table_node->props.n);  // todo: add u16::MAX check for initial properties

    const auto prop_expr_flags = bcc_flag_unset_n(flags, (uint16_t[]) { nl_bcc_cf_in_of_lhs }, 1);

    for (; new_table_entry_it < new_table_entry_end; new_table_entry_it++) {
        const auto prop_key_id = bcc_record_prop_key_imm(bcc, tk_get_lexeme(&new_table_entry_it->name, bcc->source));

        const auto prop_init_result = bcc_emit_next(bcc, new_table_entry_it->initial, prop_expr_flags);

        if (!bcc_flag_check_n(prop_init_result.flags, (uint16_t[]) { nl_bcc_cf_ok }, 1)) {
            return prop_init_result;
        }

        const auto prop_init_v_reg = bcc_normalize_to_local_reg(bcc, prop_init_result);

        if (prop_init_v_reg == -1) {
            bcc_report_err(bcc, "Could not resolve storage for field initializer. This is likely a bug.", new_table_entry_it->name.line, new_table_entry_it->name.col);

            return bcc_dud_result_v;
        }

        bcc_emit_inst_ternary(bcc, nl_op_init_prop, object_temp_reg, prop_key_id, prop_init_v_reg);
    }

    bcc_emit_inst_unary(bcc, nl_op_seal_obj, object_temp_reg);
    bcc->top_reg = object_temp_reg + 1;

    return (bcc_result) {
        .n = object_temp_reg,
        .cid = bcc->curr_cid,
        .flags = flags,
        .domain = nl_bcc_sym_local_r,
        .with_extra = false
    };
}

[[nodiscard]]
bcc_result bcc_emit_expr_access(bcc_state* bcc, const ast_node* node, uint16_t flags) {
    const auto prop_access_node = &node->data.accs;
    const ast_node* const prop_access_target = prop_access_node->target;
    // ! For assignable lvalues in property accesses, leave the last key ID as a temporary ABOVE the target object register.
    const part_access* prop_accesses = prop_access_node->accesses.data;

    
    const bool prop_accesses_in_lhs = bcc_flag_check_n(flags, (uint16_t[]) { nl_bcc_cf_in_of_lhs }, 1);
    const uint16_t prop_access_last_pos = prop_accesses_in_lhs ? prop_access_node->accesses.n - 1 : prop_access_node->accesses.n;
    const auto access_key_flags = bcc_flag_unset_n(flags, (uint16_t[]) { nl_bcc_cf_in_of_lhs }, 1);

    const auto object_target_result = bcc_emit_next(bcc, prop_access_target, access_key_flags);
    if (!bcc_flag_check_n(object_target_result.flags, (uint16_t[]) { nl_bcc_cf_ok }, 1)) {
        return object_target_result;
    }

    const uint16_t object_temp_reg = bcc_normalize_to_local_reg(bcc, object_target_result);
    const uint16_t temp_item_reg = bcc->top_reg++; // ! Reserve register just above the target object's reference to avoid losing it for possible method calls. Above the target will be a property sub-value, possibly a method.

    bcc_emit_inst_binary(bcc, nl_op_mov_rr, temp_item_reg, object_temp_reg);

    for (uint16_t prop_access_pos = 0; prop_access_pos < prop_access_last_pos; prop_access_pos++) {
        const auto next_prop_access = &prop_accesses[prop_access_pos];
        const auto prop_site_ic_id = nl_bcc_place_ic(bcc, bcc_idle_ic_v);

        if (next_prop_access->is_named) {
            const auto prop_key_imm = bcc_record_prop_key_imm(bcc, tk_get_lexeme(&next_prop_access->data.name_tk, bcc->source));
            
            bcc_emit_inst_unary(bcc, nl_op_use_ic, prop_site_ic_id);
            bcc_emit_inst_ternary(bcc, nl_op_get_prop, temp_item_reg, object_temp_reg, prop_key_imm);
        } else {
            const auto prop_comp_key_result = bcc_emit_next(bcc, next_prop_access->data.comp_key, access_key_flags);

            if (!bcc_flag_check_n(prop_comp_key_result.flags, (uint16_t[]) { nl_bcc_cf_ok }, 1)) {
                return prop_comp_key_result;
            }

            const uint16_t prop_comp_key_reg = bcc_normalize_to_local_reg(bcc, prop_comp_key_result);

            bcc_emit_inst_ternary(bcc, nl_op_get_item, temp_item_reg, object_temp_reg, prop_comp_key_reg);

            bcc->top_reg = temp_item_reg + 1;
        }

        if (prop_access_pos + 1 < prop_access_last_pos) {
            bcc_emit_inst_binary(bcc, nl_op_mov_rr, object_temp_reg, temp_item_reg);
        }
    }

    if (prop_accesses_in_lhs) {
        const auto trailing_access = &prop_accesses[prop_access_last_pos];

        if (trailing_access->is_named) {
            // ! IMPORTANT: Record last trailing key name of target.key1, .key2, ... .keyLast to later use for set_prop opcode upon property assignments.
            return (bcc_result) {
                .n = bcc_record_prop_key_imm(bcc, tk_get_lexeme(&trailing_access->data.name_tk, bcc->source)),
                .flags = flags,
                .cid = bcc->curr_cid,
                .extra_dest_reg = object_temp_reg,
                .domain = nl_bcc_sym_prop_key_imm,
                .with_extra = true,
            };
        } else {
            const auto trailing_comp_key_result = bcc_emit_next(bcc, trailing_access->data.comp_key, access_key_flags);
            if (!bcc_flag_check_n(trailing_comp_key_result.flags, (uint16_t[]) { nl_bcc_cf_ok }, 1)) {
                return trailing_comp_key_result;
            }

            const auto trailing_comp_key_reg = bcc_normalize_to_local_reg(bcc, trailing_comp_key_result);

            return (bcc_result) {
                .n = trailing_comp_key_reg,
                .flags = flags,
                .cid = bcc->curr_cid,
                .extra_dest_reg = object_temp_reg,
                .domain = nl_bcc_sym_local_r,
                .with_extra = true
            };
        }
    } else {   
        return (bcc_result) {
            .n = temp_item_reg,
            .cid = bcc->curr_cid,
            .flags = flags,
            .extra_dest_reg = object_temp_reg,
            .domain = nl_bcc_sym_local_r,
            .with_extra = true
        };
    }
}

[[nodiscard]]
bcc_result bcc_emit_expr_call(bcc_state* bcc, const ast_node* node, uint16_t flags) {
    const auto call_node = &node->data.call;
    const auto call_callee = call_node->callee;
    const auto call_argv = (const ast_node*) call_node->arg_exprs.data;
    const uint32_t call_argc = call_node->arg_exprs.n;

    if (call_argc > bcc_max_arity) {
        bcc_report_err(bcc, "The nearby call has more arguments than is supported by the interpreter (255 max).", bcc->line, bcc->col);
        return bcc_dud_result_v;
    }

    const auto not_lhs_hints = bcc_flag_unset_n(flags, (uint16_t[]) { nl_bcc_cf_in_of_lhs }, 1);

    // ? note: at runtime, each frame tracks the scope (var locator infos), caller & callee base_reg, and corresponding chunk-ID of a callee!
    // ? 1. emit evaluation of callee value... emit based on if a foreign / local reg is yielded -> local_reg...
    // ? 2. track N args & each one should be stored with `sto_arg arg-reg-id, arg-temp-reg`...
    // ? 3. call places its result into an `acc` reg for its result during runtime: it then replaces the reused callee-reg with `acc`.

    const auto callee_emit_result = bcc_emit_next(bcc, call_callee, not_lhs_hints);
    if (!bcc_flag_check_n(callee_emit_result.flags, (uint16_t[]) { nl_bcc_cf_ok }, 1)) {
        return callee_emit_result;
    }

    const auto callee_line = bcc->line, callee_col = bcc->col;

    for (uint16_t arg_pos = 0; arg_pos < call_argc; arg_pos++) {
        const auto next_arg_result = bcc_emit_next(bcc, call_argv + arg_pos, not_lhs_hints);
        if (!bcc_flag_check_n(next_arg_result.flags, (uint16_t[]) { nl_bcc_cf_ok }, 1)) {
            return next_arg_result;
        }

        bcc_emit_inst_binary(bcc, nl_op_sto_arg, arg_pos, next_arg_result.n);
    }

    const auto callee_reg = bcc_normalize_to_local_reg(bcc, callee_emit_result);

    if (callee_reg == -1) {
        bcc_report_err(bcc, "Could not resolve callee's location here.", callee_line, callee_col);

        return bcc_dud_result_v;
    }

    uint16_t accr_spill_reg;

    if (callee_emit_result.domain == nl_bcc_sym_local_r) {
        bcc_emit_inst_unary(bcc, nl_op_call, callee_emit_result.n);
        bcc_emit_inst_unary(bcc, nl_op_ld_accr, bcc->top_reg);

        accr_spill_reg = bcc->top_reg;

        bcc_update_curr_rc(bcc, bcc->top_reg++);
    } else if (callee_emit_result.domain == nl_bcc_sym_foreign_local) {
        bcc_emit_inst_ternary(bcc, nl_op_ld_upval, bcc->top_reg, callee_emit_result.n, callee_emit_result.cid);
        bcc_emit_inst_unary(bcc, nl_op_call, bcc->top_reg++);

        accr_spill_reg = bcc->top_reg;

        bcc_emit_inst_unary(bcc, nl_op_ld_accr, accr_spill_reg);
        bcc_update_curr_rc(bcc, bcc->top_reg);
    } else if (callee_emit_result.with_extra) {
        if (callee_emit_result.domain == nl_bcc_sym_prop_key_imm) {
            bcc_emit_inst_unary(bcc, nl_op_use_ic, nl_bcc_place_ic(bcc, bcc_idle_ic_v));
            bcc_emit_inst_binary(bcc, nl_op_call_method, callee_emit_result.extra_dest_reg, callee_emit_result.n);
        } else {
            bcc_report_err(bcc, "Invalid callee location found around here.", bcc->line, bcc->col);

            return bcc_dud_result_v;
        }
    } else {
        bcc_report_err(bcc, "Unresolvable location found for call result around here. This may be a bug.", bcc->line, bcc->col);

        return bcc_dud_result_v;
    }

    return (bcc_result) {
        .n = accr_spill_reg,
        .cid = bcc->curr_cid,
        .flags = flags,
        .domain = nl_bcc_sym_local_r,
        .with_extra = false
    };
}

[[nodiscard]]
bcc_result bcc_emit_unary(bcc_state* bcc, const ast_node* node, uint16_t flags) {
    const auto unary_node = &node->data.una;
    const ast_node* inner_expr = unary_node->inner;
    const auto unary_op = unary_node->op;

    const auto inner_expr_result = bcc_emit_next(bcc, inner_expr, bcc_flag_unset_n(flags, (uint16_t[]) { nl_bcc_cf_in_of_lhs }, 1));
    if (!bcc_flag_check_n(inner_expr_result.flags, (uint16_t[]) { nl_bcc_cf_ok }, 1)) {
        return inner_expr_result;
    }
    const auto inner_expr_line = bcc->line, inner_expr_col = bcc->col;

    op runtime_op = nl_op_nop;

    switch (unary_op) {
        case nl_ast_op_nullish:
            runtime_op = nl_op_test_nil;
            break;
        case nl_ast_op_negate:
            runtime_op = nl_op_neg_num;
            break;
        case nl_ast_op_logical_negate:
            runtime_op = nl_op_neg_log;
            break;
        default: break; // ! IMPORTANT: this is an unreachable case as the parser enforces the unary grammar rule.
    }

    const auto resolved_reg = bcc_normalize_to_local_reg(bcc, inner_expr_result);

    if (resolved_reg == -1) {
        bcc_report_err(bcc, "Failed to resolve inner-expression in the unary-expression around here. Did you define all names within it beforehand?", inner_expr_line, inner_expr_col);

        return bcc_dud_result_v;
    }

    bcc_emit_inst_unary(bcc, runtime_op, resolved_reg);
    bcc_update_curr_rc(bcc, bcc->top_reg);
    bcc->top_reg = resolved_reg + 1;

    return (bcc_result) {
        .n = resolved_reg,
        .cid = bcc->curr_cid,
        .flags = flags,
        .domain = nl_bcc_sym_local_r,
        .with_extra = false
    };
}

// todo: later support left-to-right operator checks.
[[nodiscard]]
bcc_result bcc_emit_binary_normal(bcc_state* bcc, const ast_node* node, uint16_t flags) {
    const auto binary_node = &node->data.bin_n;
    const ast_node* binary_lhs = binary_node->lhs, *binary_rhs = binary_node->rhs;
    const auto binary_op = binary_node->op;
    const auto binary_operand_flags = bcc_flag_unset_n(flags, (uint16_t[]) { nl_bcc_cf_in_of_lhs }, 1);

    const auto lhs_result = bcc_emit_next(bcc, binary_lhs, binary_operand_flags);

    if (!bcc_flag_check_n(lhs_result.flags, (uint16_t[]) { nl_bcc_cf_ok }, 1)) {
        return lhs_result;
    }

    const auto rhs_result = bcc_emit_next(bcc, binary_rhs, binary_operand_flags);

    if (!bcc_flag_check_n(lhs_result.flags, (uint16_t[]) { nl_bcc_cf_ok }, 1)) {
        return rhs_result;
    }

    const auto resolved_lhs_reg = bcc_normalize_to_local_reg(bcc, lhs_result), resolved_rhs_reg = bcc_normalize_to_local_reg(bcc, rhs_result);

    bcc_update_curr_rc(bcc, resolved_rhs_reg);

    op runtime_op = nl_op_nop;

    switch (binary_op) {
        case nl_ast_op_mod: runtime_op = nl_op_mod; break;
        case nl_ast_op_mul: runtime_op = nl_op_mul; break;
        case nl_ast_op_div: runtime_op = nl_op_div; break;
        case nl_ast_op_add: runtime_op = nl_op_add; break;
        case nl_ast_op_sub: runtime_op = nl_op_sub; break;
        case nl_ast_op_equals: runtime_op = nl_op_eq; break;
        case nl_ast_op_bang_equals: runtime_op = nl_op_ne; break;
        case nl_ast_op_lt: runtime_op = nl_op_lt; break;
        case nl_ast_op_gt: runtime_op = nl_op_gt; break;
        case nl_ast_op_lte: runtime_op = nl_op_lte; break;
        case nl_ast_op_gte: runtime_op = nl_op_gte; break;
        default: break;
    }

    bcc_emit_inst_ternary(bcc, runtime_op, resolved_lhs_reg, resolved_lhs_reg, resolved_rhs_reg);
    bcc_update_curr_rc(bcc, bcc->top_reg);
    bcc->top_reg = resolved_lhs_reg + 1;

    return (bcc_result) {
        .n = resolved_lhs_reg,
        .flags = binary_operand_flags,
        .cid = bcc->curr_cid,
        .domain = nl_bcc_sym_local_r,
        .with_extra = false
    };
}

[[nodiscard]]
bcc_result bcc_emit_binary_logical(bcc_state* bcc, const ast_node* node, uint16_t flags) {
    const auto bin_logical_node = &node->data.bin_l;
    const ast_node* binary_lhs = bin_logical_node->lhs, *binary_rhs = bin_logical_node->rhs;
    const auto operand_flags = bcc_flag_unset_n(flags, (uint16_t[]) { nl_bcc_cf_in_of_lhs }, 1);
    auto binary_logical_runtime_op = nl_op_nop;

    switch (bin_logical_node->op) {
        case nl_ast_op_coalesce: binary_logical_runtime_op = nl_op_jmp_not_nil; break;
        case nl_ast_op_and: binary_logical_runtime_op = nl_op_jmp_else; break;
        case nl_ast_op_or: binary_logical_runtime_op = nl_op_jmp_if; break;
        default: break;
    }

    const auto lhs_result = bcc_emit_next(bcc, binary_lhs, operand_flags);

    if (!bcc_flag_check_n(lhs_result.flags, (uint16_t[]) { nl_bcc_cf_ok }, 1)) {
        return lhs_result;
    }

    const auto resolved_lhs_reg = bcc_normalize_to_local_reg(bcc, lhs_result);
    const auto jumper_ipos = bcc_emit_inst_binary(bcc, binary_logical_runtime_op, resolved_lhs_reg, -1);

    const auto rhs_result = bcc_emit_next(bcc, binary_rhs, operand_flags);

    if (!bcc_flag_check_n(rhs_result.flags, (uint16_t[]) { nl_bcc_cf_ok }, 1)) {
        return rhs_result;
    }

    const auto resolved_rhs_reg = bcc_normalize_to_local_reg(bcc, rhs_result);
    const auto end_binary_logical_ipos = bcc_emit_inst_binary(bcc, nl_op_mov_rr, resolved_lhs_reg, resolved_rhs_reg) + 1;

    bcc_patch_cond_jump(bcc, jumper_ipos, end_binary_logical_ipos);

    bcc_update_curr_rc(bcc, bcc->top_reg);
    bcc->top_reg = resolved_lhs_reg + 1;

    return (bcc_result) {
        .n = resolved_lhs_reg,
        .cid = bcc->curr_cid,
        .flags = operand_flags,
        .domain = nl_bcc_sym_local_r,
        .with_extra = false
    };
}

[[nodiscard]]
bcc_result bcc_emit_expr_assign(bcc_state* bcc, const ast_node* node, uint16_t flags) {
    const auto assign_node = &node->data.ass;
    const auto assign_dest = (const ast_node*) assign_node->dest, assign_src = (const ast_node*) assign_node->src;
    auto assign_runtime_op = nl_op_nop;

    switch (assign_node->op) {
        case nl_ast_op_bind: assign_runtime_op = nl_op_mov_rr; break;
        case nl_ast_op_bind_mul: assign_runtime_op = nl_op_mul; break;
        case nl_ast_op_bind_div: assign_runtime_op = nl_op_div; break;
        case nl_ast_op_bind_add: assign_runtime_op = nl_op_add; break;
        case nl_ast_op_bind_sub: assign_runtime_op = nl_op_sub; break;
        default: break;
    }

    const auto dest_result = bcc_emit_next(bcc, assign_dest, bcc_flag_set_n(flags, (uint16_t[]) { nl_bcc_cf_in_of_lhs }, 1));

    if (!bcc_flag_check_n(dest_result.flags, (uint16_t[]) { nl_bcc_cf_ok }, 1)) {
        return dest_result;
    }

    const auto src_result = bcc_emit_next(bcc, assign_src, bcc_flag_unset_n(flags, (uint16_t[]) { nl_bcc_cf_in_of_lhs }, 1));
    
    if (!bcc_flag_check_n(src_result.flags, (uint16_t[]) { nl_bcc_cf_ok }, 1)) {
        return src_result;
    }

    const auto src_reg = bcc_normalize_to_local_reg(bcc, src_result);

    if (dest_result.domain == nl_bcc_sym_local_r) {
        const auto dest_reg = dest_result.n;
    
        if (assign_runtime_op == nl_op_mov_rr) {
            bcc_emit_inst_binary(bcc, assign_runtime_op, dest_reg, src_reg);
        } else {
            bcc_emit_inst_ternary(bcc, assign_runtime_op, dest_reg, dest_reg, src_reg);
        }
    
        bcc_update_curr_rc(bcc, src_reg + 1);
        bcc->top_reg = dest_reg + 1;
    } else if (dest_result.domain == nl_bcc_sym_foreign_local) {
        if (assign_runtime_op == nl_op_mov_rr) {
            bcc_emit_inst_ternary(bcc, nl_op_sto_upval, dest_result.n, dest_result.cid, src_reg);
        } else {
            // todo: support upvalue in-place MDAS opcodes...
            bcc_report_err(bcc, "Compound assignment operators are unsupported for captured names.", bcc->line, bcc->col);

            return bcc_dud_result_v;
        }
    } else if (dest_result.with_extra) {
        if (assign_runtime_op == nl_op_mov_rr) {
            if (dest_result.domain == nl_bcc_sym_prop_key_imm) {
                bcc_emit_inst_unary(bcc, nl_op_use_ic, nl_bcc_place_ic(bcc, bcc_idle_ic_v));
                bcc_emit_inst_ternary(bcc, nl_op_set_prop, dest_result.extra_dest_reg, dest_result.n, src_reg);
            } else if (dest_result.domain == nl_bcc_sym_local_r) {
                bcc_emit_inst_ternary(bcc, nl_op_set_item, dest_result.extra_dest_reg, dest_result.n, src_reg);
            } else {
                bcc_report_err(bcc, "Invalid property assignment destination found around here.", bcc->line, bcc->col);

                return bcc_dud_result_v;
            }
        } else {
            bcc_report_err(bcc, "Unsupported: Property compound assignment destination found around here.", bcc->line, bcc->col); // todo: implement emissions of in-place ModMDAS opcodes...

            return bcc_dud_result_v;
        }
    }

    return dest_result;
}

[[nodiscard]]
bcc_result bcc_emit_cond(bcc_state* bcc, const ast_node* node, uint16_t flags) {
    const auto cond_node = &node->data.cnd;
    const auto cond_entries = (const cond_entry*) cond_node->entries.data;
    const auto cond_entries_n = cond_node->entries.n;
    const auto cond_item_flags = bcc_flag_unset_n(flags, (uint16_t[]) { nl_bcc_cf_in_of_lhs }, 1);
    const auto cond_is_expr = cond_node->is_expr;

    auto cond_ending_ips = dynbuf_new(sizeof(uint16_t), 4, nullptr);
    const auto cond_result_value_reg = ++bcc->top_reg;

    for (uint16_t cond_entry_i = 0; cond_entry_i < cond_entries_n; cond_entry_i++) {
        const ast_node* const cond_entry_check = cond_entries[cond_entry_i].check;
        const ast_node* const cond_case_resultant = cond_entries[cond_entry_i].body;

        bcc->top_reg = cond_result_value_reg + 1;

        // ? LOGIC IF condition exists...
        // ? - Case 1:
        // ?   Emit check and then cond jump... Record the jump location in `next_cond_check_ipos`.
        // ?   Emit resulting expr's code and then a jump (which is recorded in `cond_ending_ips`).
        // ? Patch cond jump to go after the result expr's code.
        // ? - Case 2:
        // ?   Emit resulting expr. only.

        if (cond_entry_check) {
            const auto entry_check_result = bcc_emit_next(bcc, cond_entry_check, cond_item_flags);

            if (!bcc_flag_check_n(entry_check_result.flags, (uint16_t[]) { nl_bcc_cf_ok }, 1)) {
                return entry_check_result;
            }

            const auto entry_check_temp_reg = bcc_normalize_to_local_reg(bcc, entry_check_result);
            const auto entry_checked_jump_ipos = bcc_emit_inst_binary(bcc, nl_op_jmp_else, entry_check_temp_reg, 0);
            const auto entry_resultant_result = bcc_emit_next(bcc, cond_case_resultant, cond_item_flags);

            if (!bcc_flag_check_n(entry_resultant_result.flags, (uint16_t[]) { nl_bcc_cf_ok }, 1)) {
                dynbuf_del(&cond_ending_ips);

                return entry_resultant_result;
            }

            const auto entry_resultant_value_reg = bcc_normalize_to_local_reg(bcc, entry_resultant_result);
            
            bcc_emit_inst_binary(bcc, nl_op_mov_rr, cond_result_value_reg, entry_resultant_value_reg);
            bcc_update_curr_rc(bcc, bcc->top_reg);

            const auto next_exit_jump_ipos = bcc_emit_inst_unary(bcc, nl_op_jmp, 0);
            const auto next_entry_ipos = bcc_next_ip(bcc);

            bcc_patch_cond_jump(bcc, entry_checked_jump_ipos, next_entry_ipos);
            dynbuf_push(&cond_ending_ips, &next_exit_jump_ipos);
        } else {
            const auto last_entry_resultant_result = bcc_emit_next(bcc, cond_case_resultant, cond_item_flags);

            if (!bcc_flag_check_n(last_entry_resultant_result.flags, (uint16_t[]) { nl_bcc_cf_ok }, 1)) {
                dynbuf_del(&cond_ending_ips);

                return last_entry_resultant_result;
            }

            const auto last_entry_result_value_reg = bcc_normalize_to_local_reg(bcc, last_entry_resultant_result);

            bcc_emit_inst_binary(bcc, nl_op_mov_rr, cond_result_value_reg, last_entry_result_value_reg);
            bcc_update_curr_rc(bcc, bcc->top_reg);
        }
    }

    const auto end_cond_ipos = bcc_emit_inst_nonary(bcc, nl_op_nop);

    for (uint16_t jump_ipos_index = 0; jump_ipos_index < cond_ending_ips.n; jump_ipos_index++) {
        bcc_patch_jump(bcc, *((const uint16_t*) dynbuf_get(&cond_ending_ips, jump_ipos_index)), end_cond_ipos);
    }

    dynbuf_del(&cond_ending_ips);
    bcc->top_reg = cond_result_value_reg + 1;

    if (cond_is_expr) {
        return (bcc_result) {
            .n = cond_result_value_reg,
            .cid = bcc->curr_cid,
            .flags = flags,
            .domain = nl_bcc_sym_local_r,
            .with_extra = false
        };
    } else {
        return (bcc_result) {
            .n = 0,
            .cid = 0,
            .flags = flags,
            .domain = nl_bcc_sym_invalid,
            .with_extra = false
        };
    }
}

[[nodiscard]]
bcc_result bcc_emit_loop(bcc_state* bcc, const ast_node* node, uint16_t flags) {
    const auto loop_node = &node->data.lp;
    const ast_node* const loop_initial = loop_node->init_expr;
    const ast_node* const loop_check = loop_node->check_expr;
    const ast_node* const loop_update = loop_node->update_expr;
    const ast_node* const loop_body = loop_node->body;

    const auto loop_expr_flags = bcc_flag_unset_n(flags, (uint16_t[]) { nl_bcc_cf_in_of_lhs }, 1);
    const auto loop_temp_reg = ++bcc->top_reg;

    const auto loop_initial_result = bcc_emit_next(bcc, loop_initial, loop_expr_flags);
    if (!bcc_flag_check_n(loop_initial_result.flags, (uint16_t[]) { nl_bcc_cf_ok }, 1)) {
        return loop_initial_result;
    }

    const auto loop_initializer_reg = bcc_normalize_to_local_reg(bcc, loop_initial_result);
    bcc_emit_inst_binary(bcc, nl_op_mov_rr, loop_temp_reg, loop_initializer_reg);

    bcc_start_loop(bcc, loop_temp_reg);

    const auto loop_begin_ipos = bcc_next_ip(bcc);

    const auto loop_check_result = bcc_emit_next(bcc, loop_check, loop_expr_flags);
    if (!bcc_flag_check_n(loop_check_result.flags, (uint16_t[]) { nl_bcc_cf_ok }, 1)) {
        return loop_check_result;
    }

    const auto loop_check_reg = bcc_normalize_to_local_reg(bcc, loop_check_result);
    const auto loop_exit_ip = bcc_emit_inst_binary(bcc, nl_op_jmp_else, loop_check_reg, 0);

    const auto loop_body_result = bcc_emit_next(bcc, loop_body, loop_expr_flags);
    if (!bcc_flag_check_n(loop_body_result.flags, (uint16_t[]) { nl_bcc_cf_ok }, 1)) {
        return loop_body_result;
    }

    const auto loop_updater_ip = bcc_next_ip(bcc);
    const auto loop_update_result = bcc_emit_next(bcc, loop_update, loop_expr_flags);
    if (!bcc_flag_check_n(loop_update_result.flags, (uint16_t[]) { nl_bcc_cf_ok }, 1)) {
        return loop_update_result;
    }

    const uint16_t loop_repeater_ip = bcc_emit_inst_unary(bcc, nl_op_jmp, loop_begin_ipos), loop_ender_ipos = loop_repeater_ip + 3;

    bcc_patch_cond_jump(bcc, loop_exit_ip, loop_ender_ipos);

    // ! IMPORTANT: Patch any break or continue jumps for this loop nesting level to ensure that 
    {
        const bcc_loopinfo* const curr_loopinfo = dynbuf_last(&bcc->loops);
        const dynbuf* const breaks = &curr_loopinfo->exit_iposv, * const continues = &curr_loopinfo->rep_iposv;

        for (uint32_t brk_id = 0; brk_id < breaks->n; brk_id++) {
            const uint16_t break_ipos = *((const uint16_t*) dynbuf_get_unchecked(breaks, brk_id));

            bcc_patch_jump(bcc, break_ipos, loop_ender_ipos);
        }

        for (uint32_t ctn_id = 0; ctn_id < continues->n; ctn_id++) {
            const uint16_t continue_ipos = *((const uint16_t*) dynbuf_get_unchecked(continues, ctn_id));

            bcc_patch_jump(bcc, continue_ipos, loop_updater_ip);
        }
    }

    bcc_end_loop(bcc);
    bcc->top_reg = loop_temp_reg + 1;
    bcc_update_curr_rc(bcc, bcc->top_reg);

    return (bcc_result) {
        .n = loop_temp_reg,
        .cid = bcc->curr_cid,
        .flags = flags,
        .domain = nl_bcc_sym_local_r,
        .with_extra = false
    };
}

[[nodiscard]]
bcc_result bcc_emit_loop_abator(bcc_state* bcc, const ast_node* node, uint16_t flags) {
    bcc_loopinfo* const loop_record = dynbuf_last_mut(&bcc->loops);
    const auto abator_node = &node->data.lab;
    const ast_node* const abator_result = abator_node->inner;
    const auto abator_is_break = abator_node->is_break;

    // ! IMPORTANT: Mandate a parent loop for correct semantics of break / continue exprs. 
    if (!loop_record) {
        bcc_report_err(bcc, "Expected this break / continue to be within a loop around here.", bcc->line, bcc->col);

        return bcc_dud_result_v;
    }

    const auto loop_abator_expr_result = bcc_emit_next(bcc, abator_result, flags);
    if (!bcc_flag_check_n(loop_abator_expr_result.flags, (uint16_t[]) { nl_bcc_cf_ok }, 1)) {
        return loop_abator_expr_result;
    }

    // ? Breaks MUST update the temp accumulator register for their loop. The loop expression would "resolve" with that value at runtime.
    // ? Continues MUST update the temp loop accumulator too, allowing greater control of loop results.
    const auto loop_temp_result_reg = loop_record->acc_reg;
    const auto loop_abator_temp_reg = bcc_normalize_to_local_reg(bcc, loop_abator_expr_result);

    bcc_emit_inst_binary(bcc, nl_op_mov_rr, loop_temp_result_reg, loop_abator_temp_reg);

    const auto loop_abator_ipos = bcc_emit_inst_unary(bcc, nl_op_jmp, 0);

    if (abator_is_break) {
        dynbuf_push(&loop_record->exit_iposv, &loop_abator_ipos);
    } else {
        dynbuf_push(&loop_record->rep_iposv, &loop_abator_ipos);
    }

    return (bcc_result) {
        .n = 0,
        .cid = bcc->curr_cid,
        .flags = flags,
        .domain = nl_bcc_sym_non_locator,
        .with_extra = false
    };
}

bcc_result bcc_emit_def_binding(bcc_state* bcc, tk name_tk, const ast_node* initializer, uint16_t flags) {
    const auto var_name = tk_get_lexeme(&name_tk, bcc->source);

    if (bcc_flag_check_n(flags, (uint16_t[]) { nl_bcc_cf_pass_defs }, 1)) {
        const auto reserved_local_result = bcc_record_local(bcc, var_name);

        bcc_emit_inst_unary(bcc, nl_op_store_nil, reserved_local_result->n);
        bcc_update_curr_rc(bcc, reserved_local_result->n + 1);

        return (bcc_result) {
            .n = reserved_local_result->n,
            .cid = reserved_local_result->cid,
            .flags = flags,
            .domain = nl_bcc_sym_local_r,
            .with_extra = false
        }; 
    } else {
        // ? This info should be already recorded by the previous def visits, so no nullptr checks are needed.
        const auto found_local_info = bcc_lookup_symbol(bcc, var_name, nl_bcc_sym_local_r, 'L');

        const auto reserved_init_result = bcc_emit_next(bcc, initializer, bcc_flag_unset_n(flags, (uint16_t[]) { nl_bcc_cf_in_of_lhs }, 1));
        const auto reserved_init_reg = bcc_normalize_to_local_reg(bcc, reserved_init_result);

        bcc_emit_inst_binary(bcc, nl_op_mov_rr, found_local_info->n, reserved_init_reg);
        bcc_update_curr_rc(bcc, bcc->top_reg);

        return (bcc_result) {
            .n = found_local_info->n,
            .cid = reserved_init_result.cid,
            .flags = flags,
            .domain = nl_bcc_sym_local_r,
            .with_extra = false
        };
    }
}

[[nodiscard]]
bcc_result bcc_emit_stmt_def(bcc_state* bcc, const ast_node* node, uint16_t flags) {
    const auto def_node = &node->data.def;
    const auto def_bindings = (const def_binding*) def_node->bindings.data;
    const uint16_t def_count = def_node->bindings.n;

    for (uint16_t binding_pos = 0; binding_pos < def_count; binding_pos++) {
        bcc_emit_def_binding(bcc, def_bindings[binding_pos].name, def_bindings[binding_pos].init, flags);
    }

    return (bcc_result) {
        .n = 0,
        .cid = 0,
        .flags = flags,
        .domain = nl_bcc_sym_non_locator,
        .with_extra = false
    };
}

// ! SYNOPSIS:
// ! 1. visit only defs to track local regs for variables under `nl_bcc_cf_pass_defs`...
// ! 2. visit everything else (emit def bindings' initializer movs to prior local regs)...
// ! 3. If block is a baseline body of a lambda, use a temp register for the last value.
[[nodiscard]]
bcc_result bcc_emit_block(bcc_state* bcc, const ast_node* node, uint16_t flags) {
    const auto block_node = &node->data.bl;
    const auto block_items = (const ast_node*) block_node->items.data;
    const uint16_t block_item_count = block_node->items.n;
    const auto baseline_flags = bcc_flag_unset_n(flags, (uint16_t[]) {nl_bcc_cf_started_fun_body}, 1);

    bcc_result last_item_result = bcc_dud_result_v;
    const auto block_baseline_reg = bcc->top_reg; // ? 1st reg used in block expr / body
    // const auto block_has_trailing_return = (block_item_count > 0) ? block_items[block_item_count - 1].tag == nl_ast_stmt_ret : block_items[0].tag == nl_ast_stmt_ret; // ! maybe use this var later for block rets

    bcc_update_curr_rc(bcc, bcc->top_reg);

    const auto prepass_defs_flags = bcc_flag_set_n(baseline_flags, (uint16_t[]) { nl_bcc_cf_pass_defs }, 1);

    for (uint16_t defs_item_pos = 0; defs_item_pos < block_item_count; defs_item_pos++) {
        const auto item_tag = (block_items + defs_item_pos)->tag;

        if (item_tag == nl_ast_stmt_def || item_tag == nl_ast_block) {
            const auto temp_item_result_pass_1 = bcc_emit_next(bcc, block_items + defs_item_pos, prepass_defs_flags);

            if (!bcc_flag_check_n(temp_item_result_pass_1.flags, (uint16_t[]) { nl_bcc_cf_ok }, 1)) {
                return temp_item_result_pass_1;
            } else {
                last_item_result = temp_item_result_pass_1;
                bcc_update_curr_rc(bcc, bcc->top_reg);
            }
        }
    }

    const auto emit_remainder_flags = bcc_flag_unset_n(prepass_defs_flags, (uint16_t[]) { nl_bcc_cf_pass_defs }, 1);

    for (uint16_t item_pos = 0; item_pos < block_item_count; item_pos++) {
        const auto item_tag = (block_items + item_pos)->tag;
        const auto temp_item_result_pass_2 = bcc_emit_next(bcc, block_items + item_pos, emit_remainder_flags);

        if (!bcc_flag_check_n(temp_item_result_pass_2.flags, (uint16_t[]) { nl_bcc_cf_ok }, 1)) {
            return temp_item_result_pass_2;
        } else {
            last_item_result = temp_item_result_pass_2;
            bcc_update_curr_rc(bcc, bcc->top_reg);
        }
    }

    const auto last_value_reg = bcc_normalize_to_local_reg(bcc, last_item_result);

    if (bcc_flag_check_n(flags, (uint16_t[]) { nl_bcc_cf_started_fun_body }, 1)) {
        if (last_value_reg >= 0) {
            bcc_emit_inst_nonary(bcc, nl_op_close_upvals);
            bcc_emit_inst_unary(bcc, nl_op_ret, last_value_reg);

            return (bcc_result) {
                .n = last_value_reg,
                .cid = bcc->curr_cid,
                .flags = flags,
                .domain = nl_bcc_sym_local_r,
                .with_extra = false
            };
        } else {
            bcc_emit_inst_nonary(bcc, nl_op_close_upvals);
            bcc_emit_inst_unary(bcc, nl_op_store_nil, block_baseline_reg);
            bcc_emit_inst_unary(bcc, nl_op_ret, block_baseline_reg);

            return (bcc_result) {
                .n = block_baseline_reg,
                .cid = bcc->curr_cid,
                .flags = flags,
                .domain = nl_bcc_sym_local_r,
                .with_extra = false
            };
        }
    } else {
        if (last_value_reg >= 0) {
            bcc_emit_inst_binary(bcc, nl_op_mov_rr, block_baseline_reg, last_value_reg);
            // bcc->top_reg = block_baseline_reg + 1;

            return (bcc_result) {
                .n = block_baseline_reg,
                .cid = bcc->curr_cid,
                .flags = flags,
                .domain = nl_bcc_sym_local_r,
                .with_extra = false
            };
        } else {
            bcc_emit_inst_unary(bcc, nl_op_store_nil, block_baseline_reg);
            // bcc->top_reg = block_baseline_reg + 1;

            return (bcc_result) {
                .n = block_baseline_reg,
                .cid = bcc->curr_cid,
                .flags = flags,
                .domain = nl_bcc_sym_local_r,
                .with_extra = false
            };
        }
    }
}

[[nodiscard]]
bcc_result bcc_emit_stmt_ret(bcc_state* bcc, const ast_node* node, uint16_t flags) {
    const auto ret_node = &node->data.sr;
    const ast_node* ret_expr = ret_node->result;

    const auto ret_expr_result = bcc_emit_next(bcc, ret_expr, bcc_flag_unset_n(flags, (uint16_t[]) { nl_bcc_cf_in_of_lhs }, 1));

    const auto pre_ret_top_reg = bcc->top_reg;
    const auto reserved_ret_reg = bcc_normalize_to_local_reg(bcc, ret_expr_result);

    if (reserved_ret_reg == -1) {
        return bcc_dud_result_v;
    }

    bcc_emit_inst_nonary(bcc, nl_op_close_upvals);
    bcc_emit_inst_unary(bcc, nl_op_ret, reserved_ret_reg);
    bcc_update_curr_rc(bcc, pre_ret_top_reg); // ! save register window space despite the stack-reg-alloc per result if duped returns are written by the user!

    return (bcc_result) {
        .n = 0,
        .cid = bcc->curr_cid,
        .flags = flags,
        .domain = nl_bcc_sym_invalid,
        .with_extra = false
    };
}

[[nodiscard]]
bcc_result bcc_emit_expr_stmt(bcc_state* bcc, const ast_node* node, uint16_t flags) {
    const auto expr_stmt_node = &node->data.es;
    const ast_node* expr_stmt_inner_expr = expr_stmt_node->expr;
    const auto pre_inner_expr_top_reg = bcc->top_reg;

    const auto inner_expr_result = bcc_emit_next(bcc, expr_stmt_inner_expr, bcc_flag_unset_n(flags, (uint16_t[]) { nl_bcc_cf_in_of_lhs }, 1));

    // ! Discard registers in naked expression of expr-stmt to save register window space in the VM.
    bcc_update_curr_rc(bcc, pre_inner_expr_top_reg);

    return inner_expr_result;
}



[[nodiscard]]
bcc_state nl_bcc_state_new(int32_t max_strings, int32_t max_objects, int32_t max_closures) {
    auto scope_buf = dynbuf_new(sizeof(symscope), 4, symscope_del);

    auto loop_buf = dynbuf_new(sizeof(bcc_loopinfo), 4, bcc_loopinfo_del);

    ent_pool temp_pool_s = ent_pool_new(sizeof(string), max_strings, string_del), temp_pool_o = ent_pool_new(sizeof(metatable), max_objects, metatable_del), temp_pool_c = ent_pool_new(sizeof(closure), max_closures, dud_deleter);

    bcc_state temp_bccs = {
        .scopes = scope_buf,
        .loops = loop_buf,
        .pool_s = temp_pool_s,
        .pool_o = temp_pool_o,
        .pool_c = temp_pool_c,
        .chunks = dynbuf_new(sizeof(chunk), 4, chunk_del),
        .gvals = calloc(bcc_initial_global_cap_v, sizeof(value)),
        .natives = calloc(bcc_initial_natives_cap_v, sizeof(nl_native_t)),
        .gvals_cap = bcc_initial_global_cap_v,
        .gvals_n = 0,
        .natives_cap = bcc_initial_natives_cap_v,
        .natives_n = 0,
        .top_reg = 0,
        .curr_cid = 0,
        .entry_cid = 0,
        .errors = 0,
        .line = 1,
        .col = 1
    };

    bcc_start_scope(&temp_bccs, str_from_cstr("<global>"));

    return temp_bccs;
}

void nl_bcc_state_del(bcc_state* bcc) {
    dynbuf_del(&bcc->loops);
    dynbuf_del(&bcc->scopes);
}

[[nodiscard]]
entity_handle_mut nl_bcc_record_global_object(bcc_state* bcc, const str* name, const metatable* object) {
    auto global_obj_handle = ent_pool_add(&bcc->pool_o, object);

    if (!bcc_record_global(bcc, *name, (value) {
        .n = global_obj_handle.id,
        .tag = nl_vtag_obj
    })) {
        return (entity_handle_mut) {
            .p = nullptr,
            .id = 0
        };
    }

    return global_obj_handle;
}

bool nl_bcc_record_native_callable(bcc_state* bcc, const str* name, nl_native_t f, uint8_t arity) {
    const auto next_trampoline_chunk_id = bcc_start_chunk(bcc);

    bcc->curr_cid = next_trampoline_chunk_id;

    const auto next_native_id = bcc->natives_n;
    const auto native_capacity = bcc->natives_cap;
    const auto next_native_capacity = (native_capacity * 3) / 2;

    if (next_native_id >= native_capacity) {
        auto next_native_buf = realloc(bcc->natives, next_native_capacity * sizeof(nl_native_t));

        if (!next_native_buf) {
            NL_PANIC("Failed to grow native callable buffer here.", __FILE__, __LINE__)
        }

        bcc->natives = next_native_buf;
        bcc->natives_cap = next_native_capacity;
    }

    bcc->natives[next_native_id] = f;
    bcc->natives_n++;

    bcc_emit_inst_binary(bcc, nl_op_call_n, next_native_id, arity);
    bcc_emit_inst_unary(bcc, nl_op_ld_accr, 0);
    bcc_emit_inst_unary(bcc, nl_op_ret, 0);
    
    closure new_closure = {
        // ! IMPORTANT: native functions don't need capturing here because they can access all of the VM state via `context* c`. See `typedef nl_native_t`.
        .upvals = nullptr,
        .bc = dynbuf_get_mut(&bcc->chunks, next_trampoline_chunk_id),
        .upvals_cap = 0,
        .cid = next_trampoline_chunk_id,
        .persist = false
    };

    const auto closure_id = ent_pool_add(&bcc->pool_c, &new_closure).id;

    return bcc_record_global(bcc, *name, (value) {
        .n = closure_id,
        .tag = nl_vtag_closure
    })->tag != nl_bcc_sym_invalid;
}

[[nodiscard]]
program nl_bcc_compile_tu(bcc_state* bcc, const ast_unit* tu) {
    const auto ast_roots = (const ast_node*) tu->roots.data;
    const auto ast_roots_n = tu->roots.n;

    bcc->source = &tu->source;

    const auto top_level_main_chunk_id = bcc_start_chunk(bcc);
    bcc->curr_cid = top_level_main_chunk_id;

    bcc_result last_item_result = bcc_dud_result_v;
    const auto baseline_flags = bcc_flag_set_n(nl_bcc_cf_ok, (uint16_t[]) { nl_bcc_cf_pass_defs }, 1);
    const auto block_baseline_reg = bcc->top_reg;

    for (uint16_t defs_item_pos = 0; defs_item_pos < ast_roots_n && bcc->errors < bcc_max_errors; defs_item_pos++) {
        const auto item_tag = (ast_roots + defs_item_pos)->tag;

        if (item_tag == nl_ast_stmt_def || item_tag == nl_ast_block) {
            const auto temp_item_result_pass_1 = bcc_emit_next(bcc, ast_roots + defs_item_pos, baseline_flags);

            if (!bcc_flag_check_n(temp_item_result_pass_1.flags, (uint16_t[]) { nl_bcc_cf_ok }, 1)) {
                continue;
            } else {
                last_item_result = temp_item_result_pass_1;
                bcc_update_curr_rc(bcc, bcc->top_reg);
            }
        }
    }

    const auto remainder_pass_flags = bcc_flag_unset_n(baseline_flags, (uint16_t[]) { nl_bcc_cf_pass_defs }, 1);

    for (uint16_t defs_item_pos = 0; defs_item_pos < ast_roots_n && bcc->errors < bcc_max_errors; defs_item_pos++) {
        const auto non_def_tag = (ast_roots + defs_item_pos)->tag;
        const auto temp_item_result_pass_1 = bcc_emit_next(bcc, ast_roots + defs_item_pos, remainder_pass_flags);

        if (!bcc_flag_check_n(temp_item_result_pass_1.flags, (uint16_t[]) { nl_bcc_cf_ok }, 1)) {
            continue;
        } else {
            last_item_result = temp_item_result_pass_1;
            bcc_update_curr_rc(bcc, bcc->top_reg);
        }
    }

    const auto top_level_result_reg = bcc_normalize_to_local_reg(bcc, last_item_result);

    if (bcc->errors > 0) {
        dynbuf_del(&bcc->chunks);
        free(bcc->gvals);
        ent_pool_del(&bcc->pool_c);
        ent_pool_del(&bcc->pool_o);
        ent_pool_del(&bcc->pool_s);

        return program_dud();
    }

    if (top_level_result_reg >= 0) {
        bcc_emit_inst_unary(bcc, nl_op_ret, top_level_result_reg);
    } else {
        bcc_emit_inst_unary(bcc, nl_op_store_nil, bcc->top_reg);
        bcc_emit_inst_unary(bcc, nl_op_ret, bcc->top_reg++);
    }

    return (program) {
        .pool_s = bcc->pool_s,
        .pool_o = bcc->pool_o,
        .pool_c = bcc->pool_c,
        .chunks = bcc->chunks,
        .gv = bcc->gvals,
        .natives = bcc->natives,
        .gv_len = bcc->gvals_n,
        .gv_cap = bcc->gvals_cap,
        .natives_len = bcc->natives_n,
        .natives_cap = bcc->natives_cap,
        .entry_cid = top_level_main_chunk_id
    };
}
