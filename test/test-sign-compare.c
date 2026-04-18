// Test: signed/unsigned comparison mismatches
// Expected: HIGH/CRITICAL for sign-mismatch comparisons

#include <stddef.h>
#include <stdint.h>
#include <string.h>

void process_size(size_t size);
void process_count(int count);

void test_sign_compare() {
    int signed_count = -1;
    size_t unsigned_count = 100;
    unsigned int u = 50;
    int i = -5;

    // CRITICAL: signed vs unsigned comparison — signed_count is negative,
    // but comparison will treat it as large unsigned value
    if (signed_count < unsigned_count) {
        // This branch executes because -1 as unsigned is huge
    }

    // HIGH: signed int compared to size_t (classic bug)
    if (i < u) {
        // Always true: -5 converted to unsigned is huge
    }

    // HIGH: signed loop variable vs unsigned bound
    for (int j = 0; j < unsigned_count; j++) {
        // j will never reach unsigned_count if it's > INT_MAX
        // But also: comparison j < unsigned_count is signed vs unsigned
    }

    // MEDIUM: both unsigned — no sign issue
    unsigned int a = 10;
    unsigned int b = 20;
    if (a < b) { /* safe */ }

    // HIGH: in function argument
    process_size(signed_count);  // -1 becomes SIZE_MAX

    // MEDIUM: both signed — no sign issue
    int x = 10;
    int y = 20;
    if (x < y) { /* safe */ }
}

void test_array_index_sign() {
    int arr[100];
    int idx = -1;

    // CRITICAL: negative index (signed->unsigned in subscript)
    // idx will be converted to huge unsigned, causing out-of-bounds
    int val = arr[idx];
    (void)val;
}

void test_loop_sign_issue() {
    size_t limit = 10;
    int count = 0;

    // HIGH: signed loop var vs unsigned bound
    for (int i = 0; i < limit; i++) {
        count++;
    }

    // HIGH: unsigned loop var with signed decrement (wrap-around)
    for (unsigned int i = 10; i >= 0; i--) {
        // Infinite loop! i is unsigned, i >= 0 is always true
        // i wraps to UINT_MAX after reaching 0
        if (i == 0) break; // needs explicit break
    }
}
