#pragma once

#include <stddef.h>
#include <stdint.h>



typedef enum nl_ic_state_t : uint8_t {
    nl_ics_idle,
    nl_ics_mono,
    nl_ics_poly,
    nl_ics_dead,
} ic_state;

typedef struct nl_icache_t {
    int32_t shape_ids[4];   // cached object shape info
    int32_t recv_ids[4];    // cached property access targets by object ID 
    int32_t obj_offsets[4]; // offsets into each access target object
    uint16_t leeway;        // misses allowed between hits
    ic_state state;         // state of IC: mono --> 1 type, poly --> 4 types, or dead!
} ic;
