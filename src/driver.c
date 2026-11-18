#include "parser.h"
#include "driver.h"



typedef enum nl_driver_opt_flag_t : uint32_t {
    driver_opt_show_version = 0x1,
    driver_opt_show_help = 0x2,
    driver_opt_dump_bc = 0x4,
    driver_opt_run_bc = 0x8,
    driver_opt_show_arg_err = 0x16
} driver_opt_flag;

[[nodiscard]]
driver nl_driver_new(driver_config config) {
    auto driver_bcc_state = nl_bcc_state_new(config.max_strings, config.max_objects, config.max_closures);

    return (driver) {
        .compiler = driver_bcc_state,
        .config = config,
        .file_path = nullptr,
        .option_flags = 0x0000,
        .exit_status = 0,
    };
}

entity_handle_mut nl_driver_add_global_object(driver* d, str global_name, const metatable* object) {
    return nl_bcc_record_global_object(&d->compiler, &global_name, object);
}

void nl_driver_del(driver* d) {
    nl_bcc_state_del(&d->compiler);
}

void driver_process_argv(driver* d, const char* argv[], int arg_count) {
    auto needs_file_path = false;

    for (int arg_pos = 0; arg_pos < arg_count - 1; arg_pos++) {
        const auto arg = str_from_cstr(argv[1 + arg_pos]);

        if (str_eq_raw(&arg, "-h", 2)) {
            d->option_flags |= driver_opt_show_help;
            break;
        } else if (str_eq_raw(&arg, "-v", 2)) {
            d->option_flags |= driver_opt_show_version;
            break;
        } else if (str_eq_raw(&arg, "-d", 2)) {
            d->option_flags |= driver_opt_dump_bc;
            needs_file_path = true;
        } else if (str_eq_raw(&arg, "-r", 2)) {
            d->option_flags |= driver_opt_run_bc;
            needs_file_path = true;
        } else if (needs_file_path && !d->file_path) {
            d->file_path = arg.s;
            break;
        } else {
            d->option_flags |= driver_opt_show_arg_err;
            break;
        }
    }
}

void driver_warn_bad_args_and_exit(driver* d) {
    puts("\x1b[1;31mNeu Lang v0.0.1\x1b[0m\nBy: DrkWithT\nOptions:\n\t-h: show usage information.\n\t-d: dump bytecode or debug logs.\n\t-r: run a Neu file.\n");
}

void driver_show_help_and_exit(driver* d) {
    puts("\x1b[1;36mNeu Lang v0.0.1\x1b[0m\nBy: DrkWithT\nOptions:\n\t-h: show usage information.\n\t-d: dump bytecode or debug logs.\n\t-r: run a Neu file.\n");
}

void driver_show_version_and_exit(driver* d) {
    const auto config_ref = &d->config;

    fprintf(stdout, "\x1b[1;36m%s v%u.%u.%u\x1b[0m -- By: DrkWithT (GitHub)\n", config_ref->codename_cstr_literal, config_ref->version_major, config_ref->version_minor, config_ref->version_patch);
}

[[nodiscard]]
bool driver_exec_bc(driver* d, program* pg) {
    // todo: implement VM
    return false;
}

[[nodiscard]]
static string read_file(const char* file_path) {
    constexpr size_t chomp_size = 64;

    FILE* reader = fopen(file_path, "r");
    auto s = string_dud();

    if (!reader) {
        return s;
    }

    char chomp_buffer[chomp_size + 1] = {0};
    size_t temp_rc;

    while (!feof(reader)) {
        temp_rc = fread(chomp_buffer, sizeof(char), chomp_size, reader);

        if (temp_rc == 0) {
            break;
        }

        string_append_raw(&s, chomp_buffer, temp_rc);
    }

    return s;
}

[[nodiscard]]
int nl_driver_run(driver* d, const char* argv[], int arg_count) {
    driver_process_argv(d, argv, arg_count);

    if (d->option_flags & driver_opt_show_arg_err) {
        return 1;
    } else if (d->option_flags & driver_opt_show_help) {
        driver_show_help_and_exit(d);

        return 0;
    } else if (d->option_flags & driver_opt_show_version) {
        driver_show_version_and_exit(d);

        return 0;
    }

    auto file_source = read_file(d->file_path);

    if (string_empty(&file_source)) {
        puts("\x1b[1;33mNOTE:\x1b[0m Did you pass a valid .neu file?");
        return 1;
    }

    const auto file_view = str_from_string(&file_source);
    auto ast = nl_parse_source(d->lexicals, &file_view);

    if (str_empty(&ast.source)) {
        puts("\x1b[1;31mERROR:\x1b[0m Failed to parse given source.");

        string_del(&file_source);

        return 1;
    }

    auto bc_program = nl_bcc_compile_tu(&d->compiler, &ast);

    if (!bc_program.gv) {
        puts("\x1b[1;31mERROR:\x1b[0m Failed to successfully compile script.");

        program_del(&bc_program);
        ast_unit_del(&ast);
        string_del(&file_source);

        return 1;
    }

    if (d->option_flags & driver_opt_dump_bc) {
        program_output(stdout, &bc_program);
    } else if (!driver_exec_bc(d, &bc_program)) {
        puts("\x1b[1;31mERROR:\x1b[0m Failed to successfully execute script.");

        program_del(&bc_program);
        ast_unit_del(&ast);
        string_del(&file_source);

        return 1;
    }

    return 0;
}
