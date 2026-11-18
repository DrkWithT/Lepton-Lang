#pragma once



#include "basics.h"

typedef struct nl_string_t string;

/// ASCII string view
typedef struct nl_str_t {
    const char* s;
    size_t n;
} str;

[[nodiscard]]
str str_dud();

[[nodiscard]]
str str_new(const char* base, size_t len);

[[nodiscard]]
str str_from_cstr(const char* cstr);

[[nodiscard]]
str str_from_string(const string* s);

[[deprecated("This is a no-op function because str does not own its data.")]]
NL_CXX_INLINE void str_del([[maybe_unused]] str* self) {}

[[nodiscard]]
NL_CXX_INLINE bool str_empty(const str* self) {
    return self->n == 0 || !self->s;
}

[[nodiscard]]
NL_CXX_INLINE size_t str_size(const str* self) {
    return self->n;
}

[[nodiscard]]
int32_t str_index_of(const str* self, char c);

[[nodiscard]]
bool str_eq_raw(const str* self, const char* cstr, size_t n);

[[nodiscard]]
bool str_eq_str(const str* self, const str* rhs);

void str_output(FILE* fs, const str* s);

[[nodiscard]]
int32_t str_to_i32(const str* s);

[[nodiscard]]
float str_to_f32(const str* s);


/// owning, ASCII string
typedef struct nl_string_t {
    char* data;
    size_t n;
    size_t cap;
} string;

[[nodiscard]]
string string_dud();

[[nodiscard]]
string string_new(const char* cstr, size_t initial_len);

[[nodiscard]]
string string_from_str(const str* sv);

void string_del(void* self);

[[nodiscard]]
NL_CXX_INLINE bool string_empty(const string* self) {
    return !self->data || self->n == 0;
}

[[nodiscard]]
NL_CXX_INLINE size_t string_size(const string* self) {
    return self->n;
}

[[nodiscard]]
NL_CXX_INLINE const char* string_data(const string* self) {
    return self->data;
}

[[nodiscard]]
NL_CXX_INLINE char* string_data_mut(string* self) {
    return self->data;
}

[[nodiscard]]
bool string_eq_str(const string* self, const str* s);

[[nodiscard]]
bool string_eq_string(const string* self, const string* rhs);

void string_append_raw(string* self, const char* s, size_t n);

void string_append_str(string* self, const str* rhs);

void string_output(FILE* fs, const string* s);
