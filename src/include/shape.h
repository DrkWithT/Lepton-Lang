#pragma once

#include "dynbuf.h"



typedef enum nl_prop_flag_t : uint8_t {
    nl_prop_invalid = 0x1,
    nl_prop_writable = 0x2,
    nl_prop_may_del = 0x4,
    nl_prop_default = nl_prop_writable | nl_prop_may_del
} prop_flag;

typedef struct nl_shape_ent_t {
    int32_t key_sid;        // holds string `ID` of name
    uint16_t offset;        // holds property value offset
    uint8_t meta;          // holds property flags
} shape_ent;

typedef struct nl_shape_t {
    dynbuf entries;         // stores `shape_ent[]`
    int32_t id;             // ID into pool manager of all shapes
} shape;
