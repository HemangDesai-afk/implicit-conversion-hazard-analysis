// Test: narrowing conversions in assignments and function calls
// Expected: HIGH risk for narrowing in function args, MEDIUM for assignments

#include <stddef.h>
#include <stdint.h>

void take_short(short s);
void take_char(char c);
void take_unsigned_short(unsigned short us);
void take_int32(int32_t x);
void take_uint32(uint32_t x);

void test_narrowing() {
    long long big_value = 0x7FFFFFFFFFFFFFFFLL;

    // HIGH: narrowing long long -> short in function argument
    take_short((short)big_value);

    // MEDIUM: narrowing long long -> int in assignment
    int truncated = big_value;

    // HIGH: int -> char (severe narrowing)
    char small = big_value;
    take_char(small);

    // MEDIUM: signed -> unsigned conversion
    int signed_val = -1;
    unsigned int unsigned_val = signed_val;
    take_unsigned_short(unsigned_val);

    // LOW: literal fits in target (but still flagged)
    int from_literal = 42;
    char safe_char = 42;

    // HIGH: potential truncation in API boundary
    take_int32((int32_t)big_value);

    // MEDIUM: widening is generally safe, but still implicit
    long wider = from_literal;

    // HIGH: int -> uint32_t with potentially negative source
    take_uint32(signed_val);
}

void test_pointer_to_int() {
    int* ptr = 0;

    // HIGH: pointer to integer (portability bug on 64-bit)
    uintptr_t addr = (uintptr_t)ptr;

    // CRITICAL: integer to pointer (very dangerous)
    int* bad_ptr = (int*)addr;
    (void)bad_ptr;
}
