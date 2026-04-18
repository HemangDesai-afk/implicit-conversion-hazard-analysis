// Test: enum-to-integer conversions in switch statements
// Expected: MEDIUM/HIGH for enum in switch, especially with unscoped enums

#include <stdio.h>

// Unscoped enum (C-style) — implicitly converts to int
enum Color {
    COLOR_RED,
    COLOR_GREEN,
    COLOR_BLUE
};

// Another unscoped enum with overlapping values
enum TrafficLight {
    LIGHT_RED,    // same value as COLOR_RED = 0
    LIGHT_YELLOW, // = 1
    LIGHT_GREEN   // = 2
};

enum Status {
    STATUS_OK = 0,
    STATUS_ERROR = -1,  // negative value — can't fit in unsigned
    STATUS_PENDING = 1
};

void handle_color(int color_value) {
    // MEDIUM: int in switch — could be any value, not just enum
    switch (color_value) {
        case COLOR_RED:
            printf("red\n");
            break;
        case COLOR_GREEN:
            printf("green\n");
            break;
        case COLOR_BLUE:
            printf("blue\n");
            break;
        // No default: what if color_value is 99?
    }
}

void handle_status(enum Status status) {
    // HIGH: enum-to-int implicit conversion in switch
    // The enum value is implicitly converted to int for the switch
    switch (status) {
        case STATUS_OK:
            break;
        case STATUS_ERROR:
            break;
        // Missing STATUS_PENDING — silent bug if status is PENDING
    }
}

// Function taking int where enum expected
void set_color(int c) {
    // Caller might pass non-enum values
    enum Color color = c;  // HIGH: int-to-enum implicit conversion
    (void)color;
}

// Return enum from function with int return type
enum Color get_default_color() {
    return COLOR_RED;  // OK: enum returned as enum
}

int get_color_as_int() {
    return COLOR_RED;  // MEDIUM: enum-to-int in return (implicit)
}

void test_enum_arithmetic() {
    enum Color c1 = COLOR_RED;
    enum Color c2 = COLOR_GREEN;

    // MEDIUM: enum-to-int in arithmetic
    int sum = c1 + c2;  // enums implicitly converted to int
    (void)sum;

    // HIGH: int result assigned to enum variable
    enum Color c3 = c1 + 1;  // int-to-enum conversion
    (void)c3;
}

void test_overlapping_enums() {
    enum Color color = COLOR_RED;     // 0
    enum TrafficLight light = LIGHT_RED;  // also 0

    // HIGH: comparing values from different enums (same underlying int)
    if (color == light) {
        // True, but semantically wrong — different types!
    }

    // MEDIUM: assigning between different enum types via int
    int value = color;
    enum Color converted = value;
    (void)converted;
}
