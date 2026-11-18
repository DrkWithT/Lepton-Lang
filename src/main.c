#include "driver.h"



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

    const auto status = nl_driver_run(&neu_app, argv, argc);

    nl_driver_del(&neu_app);

    return status;
}
