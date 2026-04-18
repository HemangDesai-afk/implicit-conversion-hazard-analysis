// Test: float-to-int conversions in loop bounds and array indexing
// Expected: HIGH/CRITICAL for float loop bounds and float array indices

#include <math.h>

void process(int n);

void test_float_loop_bound() {
    double total = 100.5;

    // CRITICAL: float-to-int in loop bound — truncates to 100
    for (int i = 0; i < total; i++) {
        // The condition `i < total` implicitly converts i to double,
        // then back to int for comparison — can cause off-by-one
    }

    // HIGH: float loop variable with int bound
    float limit = 50.0f;
    for (float f = 0.0f; f < limit; f += 1.0f) {
        // Floating point accumulation causes drift
    }

    // MEDIUM: double in arithmetic with int
    int count = 10;
    double result = count * 1.5;  // int promoted to double
    int truncated = result;        // double to int — truncation
}

void test_float_array_index() {
    int arr[100];
    double idx = 3.7;

    // CRITICAL: float-to-int in array index — truncates to 3
    int val = arr[idx];  // implicit double->int conversion
    (void)val;

    // HIGH: float computation used as index
    double computed = floor(5.9);
    int indexed = arr[computed];  // truncates to 5
    (void)indexed;
}

void test_float_arithmetic() {
    int a = 10;
    double b = 3.14159;

    // MEDIUM: int promoted to double in multiplication
    double product = a * b;

    // HIGH: double to int truncation
    int result = a * b;  // 31.4159 -> 31

    // CRITICAL: float loop bound with increment
    for (double d = 0.0; d < 1000000.0; d += 0.1) {
        // After many iterations, floating-point error accumulates
        // d may never exactly equal the bound
    }
}

void test_float_return() {
    double precise = 42.9;

    // MEDIUM: return value truncation
    // If function returns int, this truncates to 42
}

int get_int(void) {
    double val = 42.9;
    return val;  // MEDIUM: implicit double->int in return
}
