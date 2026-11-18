#pragma once

#include "strs.h"
#include "lexer.h"
#include "bcc.h"



typedef struct nl_metatable_t metatable;

/**
 ### ABOUT

 Fwd. declaration for driver state. The driver is the encapsulation of all Neu language implementation details, wrapping the bytecode compiler, interpreter, and possibly a JIT in the far future.
 */
typedef struct nl_driver_t driver;

/**
 ### ABOUT

 Configuration arguments to the "Neu" driver. These can affect memory overhead of how many objects, etc. are stored by the VM.
 */
typedef struct nl_driver_config_t {
    const char* codename_cstr_literal;
    uint32_t version_major;
    uint32_t version_minor;
    uint32_t version_patch;
    uint32_t max_strings;
    uint32_t max_objects;
    uint32_t max_closures;
} driver_config;

typedef struct nl_driver_t {
    bcc_state compiler;
    driver_config config;
    const lex_item* lexicals;
    const char* file_path; // todo: replace with file-path dynbuf and file_source dynbuf...
    uint32_t option_flags;
    int exit_status;
} driver;

[[nodiscard]]
driver nl_driver_new(driver_config config);

entity_handle_mut nl_driver_add_global_object(driver* d, str global_name, const metatable* object);

void nl_driver_del(driver* d);

[[nodiscard]]
int nl_driver_run(driver* d, const char* argv[], int arg_count);
