#pragma once

#include "dynbuf.h"
#include "pool.h"
#include "value.h"
#include "ic.h"



/// Fwd. decl for Neu VM context type.
typedef struct nl_context_t context;

/// Fwd. decl for builtin, native functions in the Neu VM.
typedef bool(*nl_native_t)(context* c, uint8_t argc);

typedef enum nl_op_t : uint8_t {
    nl_op_nop,          // pc++
    nl_op_store_nil,    // store_nil dest
    nl_op_store_bool,   // store_bool dest u8-imm
    nl_op_swap_r,       // swap dest src
    nl_op_loadi_u16,    // mov dest (i32)u16-imm
    nl_op_mov_rk,       // mov dest constant-id
    nl_op_mov_rr,       // mov dest src
    nl_op_mov_rg,       // mov_global dest <global-ID>
    nl_op_sto_arg,      // mov_arg_slot <arg-ID (up to 255)> src
    nl_op_ld_arg,       // load_arg_slot dest <arg-ID>
    nl_op_ld_upval,     // load_upvar dest <upval-ID> <upval-CID>
    nl_op_sto_upval,    // store_upvar <upval-ID> <upval-CID> src
    nl_op_close_upvals, // close_upvalues
    nl_op_use_ic,       // use_ic <ID>
    nl_op_inst_trait,   // inst_trait dest trait-obj-var-src
    nl_op_seal_obj,     // seal_obj dest
    nl_op_init_prop,    // init_prop dest key-id initializer-src
    nl_op_set_prop,     // set_prop dest key-id src
    nl_op_get_prop,     // get_prop dest obj-reg key-id
    nl_op_set_item,     // (for computed props) set_item dest comp-key-reg src
    nl_op_get_item,     // (for computed props) get_item dest src-obj-reg comp-key-reg
    nl_op_ref_self,     // ref_self dest
    nl_op_ref_super,    // ref_super dest
    nl_op_ld_accr,      // load_accr dest
    nl_op_test_nil,     // test_nil dest
    nl_op_neg_num,      // numeric_neg dest
    nl_op_neg_log,      // logical_neg dest
    nl_op_mod,          // op_xxx dest a1 a2...
    nl_op_mul,
    nl_op_div,
    nl_op_add,
    nl_op_sub,
    nl_op_lt,
    nl_op_lte,
    nl_op_gt,
    nl_op_gte,
    nl_op_eq,
    nl_op_ne,
    nl_op_jmp_not_nil,  // jmp_not_nil src pc
    nl_op_jmp_if,       // jmp_if src pc
    nl_op_jmp_else,     // jmp_else src pc
    nl_op_jmp,          // jmp pc
    nl_op_mk_closure,   // mk_closure dest <chunk-ID>
    nl_op_call,         // call func-reg argc -> `accr`
    nl_op_call_n,       // call_native native-id-imm argc -> `accr`
    nl_op_ret,          // ret src
} op;

typedef struct nl_chunk_t {
    uint32_t* upval_entries;    // list of masked `u32`s: lower u16 has CID, upper has slot-ID
    uint8_t* bc;        // bytecode
    value* kd;          // constants data
    ic* icv;            // property-access inline caches
    uint16_t code_len;  // `inst` count
    uint16_t code_cap;  // `inst` capacity
    uint16_t k_len;
    uint16_t k_cap;
    uint16_t ic_len;
    uint16_t ic_cap;
    uint16_t regs_n;
    uint16_t upvals_n;
    uint8_t arity;
} chunk;

void chunk_del(void* c);

typedef struct nl_program_t {
    ent_pool pool_s;    // interned strings
    ent_pool pool_o;    // objects (metatables)
    ent_pool pool_c;    // closures
    dynbuf chunks;
    value* gv;          // global values buffer
    nl_native_t* natives;   // buffer of native function pointers
    uint16_t gv_len;
    uint16_t gv_cap;
    uint16_t natives_len;
    uint16_t natives_cap;
    uint16_t entry_cid;
} program;

[[nodiscard]]
program program_dud();

void program_del(void* pg);

void program_output(FILE* os, const program* pg);
