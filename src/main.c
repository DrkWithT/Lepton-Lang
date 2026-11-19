#include "driver.h"
#include "context.h"
#include "var_scope.h"



const lex_item neu_lexical_specials[] = {
    (lex_item) { .s = {.s = "def", .n = 3}, .tag = nl_tk_kw_def},
    (lex_item) { .s = {.s = "fun", .n = 3}, .tag = nl_tk_kw_fun},
    (lex_item) { .s = {.s = "cond", .n = 4}, .tag = nl_tk_kw_cond },
    (lex_item) { .s = {.s = "else", .n = 4}, .tag = nl_tk_kw_else },
    (lex_item) { .s = {.s = "loop", .n = 4}, .tag = nl_tk_kw_loop },
    (lex_item) { .s = {.s = "break", .n = 5}, .tag = nl_tk_kw_break },
    (lex_item) { .s = {.s = "continue", .n = 8}, .tag = nl_tk_kw_continue },
    (lex_item) { .s = {.s = "ret", .n = 3}, .tag = nl_tk_kw_ret },
    (lex_item) { .s = {.s = "nil", .n = 3}, .tag = nl_tk_kw_nil },
    (lex_item) { .s = {.s = "true", .n = 4}, .tag = nl_tk_kw_true },
    (lex_item) { .s = {.s = "false", .n = 5}, .tag = nl_tk_kw_false },
    (lex_item) { .s = {.s = "?", .n = 1}, .tag = nl_tk_op_nullish },
    (lex_item) { .s = {.s = "??", .n = 2}, .tag = nl_tk_op_coalesce },
    (lex_item) { .s = {.s = "%", .n = 1}, .tag = nl_tk_op_mod },
    (lex_item) { .s = {.s = "*", .n = 1}, .tag = nl_tk_op_mul },
    (lex_item) { .s = {.s = "/", .n = 1}, .tag = nl_tk_op_div },
    (lex_item) { .s = {.s = "+", .n = 1}, .tag = nl_tk_op_add },
    (lex_item) { .s = {.s = "-", .n = 1}, .tag = nl_tk_op_sub },
    (lex_item) { .s = {.s = "==", .n = 2}, .tag = nl_tk_op_equals },
    (lex_item) { .s = {.s = "!=", .n = 2}, .tag = nl_tk_op_bang_equals },
    (lex_item) { .s = {.s = "<", .n = 1}, .tag = nl_tk_op_lt },
    (lex_item) { .s = {.s = ">", .n = 1}, .tag = nl_tk_op_gt },
    (lex_item) { .s = {.s = "<=", .n = 2}, .tag = nl_tk_op_lte },
    (lex_item) { .s = {.s = ">=", .n = 2}, .tag = nl_tk_op_gte },
    (lex_item) { .s = {.s = "&&", .n = 2}, .tag = nl_tk_op_and },
    (lex_item) { .s = {.s = "||", .n = 2}, .tag = nl_tk_op_or },
    (lex_item) { .s = {.s = "=", .n = 1}, .tag = nl_tk_op_bind },
    (lex_item) { .s = {.s = "*=", .n = 2}, .tag = nl_tk_op_bind_mul },
    (lex_item) { .s = {.s = "/=", .n = 2}, .tag = nl_tk_op_bind_div },
    (lex_item) { .s = {.s = "+=", .n = 2}, .tag = nl_tk_op_bind_add },
    (lex_item) { .s = {.s = "-=", .n = 2}, .tag = nl_tk_op_bind_sub },
    (lex_item) { .s = {.s = "->", .n = 2}, .tag = nl_tk_arrow },
    (lex_item) { .s = {.s = nullptr, .n = 0}, .tag = nl_tk_unknown }
};

bool neu_native_assert(context* c, uint8_t arg_count) {
    const auto tested_value = c->arg_regs[0];
    const auto failure_msg = c->arg_regs[1];

    if (tested_value.tag != nl_vtag_bool || tested_value.b != 0x01) {
        const auto msg_string_handle = ent_pool_get(&c->pool_s, failure_msg.n);
        const auto msg_string_p = (const string*) msg_string_handle.p;

        if (msg_string_p) {
            fprintf(stdout, "\x1b[1;31mASSERT FAILED:\x1b[0m ");
            string_output(stdout, msg_string_p);
            fprintf(stdout, "\n");
        } else {
            fprintf(stderr, "\x1b[1;31mASSERT FAILED:\x1b[0m Expected true value here.\n");
        }
        
        return false;
    }

    auto curr_scope_frame = (scope*) dynbuf_last_mut(&c->scopes);

    curr_scope_frame->vbase[0] = (value) { .b = 0, .tag = nl_vtag_nil };

    return true;
}

int main(int argc, const char* argv[]) {
    auto neu_app = nl_driver_new((driver_config) {
        .codename_cstr_literal = "NeuLang",
        .neu_lexicals = neu_lexical_specials,
        .version_major = 0,
        .version_minor = 1,
        .version_patch = 0,
        .max_strings = 1024,
        .max_objects = 16384,
        .max_closures = 8192
    });

    nl_driver_add_native_function(&neu_app, (str) { .s = "assert", .n = 6 }, neu_native_assert, 2);

    const auto status = nl_driver_run(&neu_app, argv, argc);

    nl_driver_del(&neu_app);

    return status;
}
