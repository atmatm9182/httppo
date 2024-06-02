#pragma once

#include <stdint.h>
#include <stdbool.h>

uint64_t hash_djb2(void const* str);

bool hash_str_eq(void const* lhs, void const* rhs);
