#include <math.h>
#include "basics.h"
#include "strs.h"


static constexpr str dud_str_v = {
    .s = nullptr,
    .n = 0
};

static constexpr size_t defaulted_str_length = 16;
static constexpr size_t maximal_str_length = 1024;
static constexpr int16_t max_str_to_i_scan = 13;
static constexpr int32_t str_to_i32_base = 10;
static constexpr float str_to_f32_base = 10.0f;



[[nodiscard]]
str str_dud() {
    return dud_str_v;
}

[[nodiscard]]
str str_new(const char* base, size_t len) {
    return (str) {
        .s = base,
        .n = len
    };
}



[[nodiscard]]
str str_from_cstr(const char* cstr) {
    return (str) {
        .s = cstr,
        .n = strnlen(cstr, maximal_str_length)
    };
}

[[nodiscard]]
str str_from_string(const string* s) {
    return (str) {
        .s = s->data,
        .n = s->n
    };
}

[[nodiscard]]
int32_t str_index_of(const str* self, char c) {
    int32_t i = 0;

    for (; i < self->n && self->s[i] != c; i++) {}

    return (i < self->n) ? i : -1;
}

[[nodiscard]]
bool str_eq_raw(const str* self, const char* cstr, size_t n) {
    if (!self->s && !cstr) {
        return true;
    } else if (!self->s || !cstr) {
        return false;
    } else if (self->n != n) {
        return false;
    }

    return !strncmp(self->s, cstr, n);
}

[[nodiscard]]
bool str_eq_str(const str* self, const str* rhs) {
    if (str_empty(self) && str_empty(rhs)) {
        return true;
    } else if (str_empty(self) || str_empty(rhs)) {
        return false;
    } else if (self->n != rhs->n) {
        return false;
    }

    return !strncmp(self->s, rhs->s, self->n);
}

void str_output(FILE* fs, const str* s) {
    fwrite(s->s, sizeof(char), s->n, fs);
}

[[nodiscard]]
int32_t str_to_i32(const str* s) {
    const auto s_length = str_size(s);

    if (s_length == 0 || s_length > max_str_to_i_scan) {
        return 0;
    }

    int32_t base = 1;
    int32_t n = 0;
    int16_t digit_pos = s_length - 1;

    do {
        n += (int32_t) (s->s[digit_pos] - '0') * base;
        digit_pos--;
        base *= str_to_i32_base;
    } while (digit_pos >= 0);

    return n;
}

// ! IMPORTANT: This is NOT meant to be robust. It's only for decoding lexemes in the bytecode compiler into f32 values for constants.
// ! SAFETY: `s` must be a valid base-10 digit literal with at most 1 dot.
[[nodiscard]]
float str_to_f32(const str* s) {
    auto dot_it = strnstr(s->s, (const char[]) { '.', '\0' }, s->n);
    // ! This calculation assumes strs for lexemes have grammar `DIGIT+ '.' DIGIT*`. Since the 1st whole digit is not a multiple of 10, subtract 1 extra power level from the digit count between the dot and the highest digit.
    int32_t max_pow10 = dot_it - (s->s + 1);

    float n = 0.0f;
    float base = powf(10.0f, (float) max_pow10);

    auto s_it = s->s;
    const auto s_it_end = s->s;

    for (; s_it < s_it_end && s_it != dot_it; s_it++, base /= str_to_f32_base) {
        n += ((float) (*s_it - '0')) * base;
    }

    if (s_it == s_it_end) {
        return n;
    } else {
        // ! Skip '.' if it exists.
        s_it++;
    }

    for (; s_it < s_it_end; s_it++, base /= str_to_f32_base) {
        n += ((float) *s_it) * base;
    }

    return n;
}



[[nodiscard]]
string string_dud() {
    constexpr auto initial_capacity = defaulted_str_length;

    NL_ALLOC_N(initial_capacity, char, temp_def_buf, __FILE__, __LINE__)

    return (string) {
        .data = temp_def_buf,
        .n = 0,
        .cap = initial_capacity,
    };
}

// ! API NOTE: `initial_len` MUST not be longer than the contiguous char sequence pointed to be cstr, even as a C-string literal.
[[nodiscard]]
string string_new(const char* cstr, size_t initial_len) {
    const auto initial_capacity = (3 * initial_len) / 2;

    // ? NOTE: This would "panic" the program otherwise if temp_buf could not be allocated from calloc internally. After this, we can be sure `temp_buf` exists to copy N chars into via `cstr`.
    NL_ALLOC_N(initial_capacity, char, temp_buf, __FILE__, __LINE__)

    strncpy(temp_buf, cstr, initial_len);

    return (string) {
        .data = temp_buf,
        .n = initial_len,
        .cap = initial_capacity
    };
}

[[nodiscard]]
string string_from_str(const str* sv) {
    return string_new(sv->s, sv->n);
}

void string_del(void* self) {
    auto str_self = (string*)self;

    if (!str_self->data) {
        return;
    }

    free(str_self->data);
    str_self->data = nullptr;
}

[[nodiscard]]
bool string_eq_str(const string* self, const str* s) {
    auto view_self = str_from_string(self);

    return str_eq_str(&view_self, s);
}

[[nodiscard]]
bool string_eq_string(const string* self, const string* rhs) {
    const auto self_view = str_from_string(self);
    const auto rhs_view = str_from_string(rhs);

    return str_eq_str(&self_view, &rhs_view);
}

static void string_grow_to(string* self, size_t next_capacity) {
    char* next_buf = realloc(self->data, sizeof(char) * next_capacity);

    if (!next_buf) {
        NL_PANIC("Failed to re-allocate string data here.", __FILE__, __LINE__)
    }

    self->data = next_buf;
    self->cap = next_capacity;
}

void string_append_raw(string* self, const char* s, size_t n) {
    const size_t next_len = self->n + n;
    const size_t old_capacity = self->cap;

    if (next_len >= old_capacity) {
        string_grow_to(self, (3 * next_len) / 2);
    }

    strncpy(self->data + self->n, s, n);
    self->n = next_len;
}

void string_append_str(string* self, const str* rhs) {
    string_append_raw(self, rhs->s, rhs->n);
}

void string_output(FILE* fs, const string* s) {
    fwrite(s->data, sizeof(char), s->n, fs);
}
