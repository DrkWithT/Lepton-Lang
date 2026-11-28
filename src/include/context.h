#pragma once

#include "bytecode.h"
#include "closures.h"
#include "shape_manager.h"



typedef enum nl_context_flags_t : uint16_t {
    nl_ctxf_pending = 0x1,
    nl_ctxf_ok = 0x2,
    nl_ctxf_err_ref = 0x4,
    nl_ctxf_err_mem = 0x8,
    nl_ctxf_err_op = 0x10
} context_flags;

typedef struct nl_context_frame_t {
    const uint8_t* caller_base_ip;
    const uint8_t* caller_ip;
    const value* caller_kp;
    ic* caller_icaches;
    value* caller_stack_base;
    int32_t closure_id;
    int32_t self_obj_id;
    uint16_t cid;
} context_frame;

/**
 ### ABOUT

 This data type encapsulates the entire runtime state of the Neu language. It is meant to hold VM state for managing memory, execution location, call frames, etc. Also, native functions which are added to the interpreter MUST take in a pointer to this state.

 */
// TODO: Add shape_manager into context before implementing object handling methods.
typedef struct nl_context_t {
    ent_pool pool_s;    // interned strings
    ent_pool pool_o;    // objects (metatables)
    ent_pool pool_c;    // closures
    shm shapes;         // shape manager
    dynbuf chunks;
    dynbuf frames;
    value* stack;       // all register slots
    value* gv;          // global values buffer
    value* arg_regs;    // scratch registers for up to 255 argument values per callee
    const uint8_t* base_ip;     // code start of chunk
    const uint8_t* ip;  // points to first byte of encoded instruction
    const value* kp;    // points to chunk constants
    ic* icp;            // points to chunk ICs
    value* stack_bp;
    nl_native_t* natives;   // points to buffer of native function pointers
    value accr;         // return accumulator "register"
    uint32_t stack_sz;
    uint16_t gv_len;    // global value count (determined at bytecode compilation)
    uint16_t gv_cap;    // global value capacity (determined at bytecode compilation)
    uint16_t flags;
    uint16_t curr_ic_id;
} context;

[[nodiscard]]
context context_new(program pg, size_t shape_capacity);

void context_del(context* c);

[[nodiscard]]
value context_try_closurify(context* c, uint16_t chunk_id);

bool context_try_invoke(context* c, value callee, int32_t maybe_self_id, uint16_t ret_ip_nudge);

bool context_try_invoke_method(context* c, value caller_obj, int16_t prop_method_id);

[[nodiscard]]
entity_handle_mut context_create_with_trait(context* c, value trait_v, uint16_t initial_named_props_n);

void context_seal_object(context* c, value object_v);

void context_init_property(context* c, value object_v, uint16_t key_id, value initial_v);

[[nodiscard]]
bool context_set_property(context* c, value object_v, uint16_t key_id, value v);

[[nodiscard]]
value* context_get_property(context* c, value object_v, uint16_t key_id);

[[nodiscard]]
bool context_set_computed_property(context* c, value object_v, value key_v, value v);

[[nodiscard]]
value* context_get_computed_property(context* c, value object_v, value key_v);

[[nodiscard]]
bool context_convert_to_bool(const context* c, value v);

[[nodiscard]]
int32_t context_convert_to_i32(const context* c, value v);

[[nodiscard]]
float context_convert_to_f32(const context* c, value v);

[[nodiscard]]
bool context_test_equality(const context* c, value lhs, value rhs);

[[nodiscard]]
const value* context_get_upvalue(const context* c, uint16_t id);

void context_set_upvalue(context* c, uint16_t id, value v);

void context_close_upvalues(context* c, int32_t closure_id);
