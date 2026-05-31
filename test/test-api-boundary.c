// Test: Implicit conversions at function/API boundaries
// Expected: HIGH/CRITICAL for silent truncation or sign-flip at API boundaries

#include <stddef.h>
#include <sys/types.h>

// Simulated external library APIs
void send_packet(size_t packet_size, const char* data);
int process_offset(short offset);
void set_flag(unsigned char flag);

void test_api_sign_mismatch() {
    int requested_size = -10;
    const char* payload = "API Boundary Test";

    // CRITICAL: Passing signed int (-10) to size_t (unsigned)
    // Under the hood, -10 becomes a massive positive value, causing massive OOB read/write in send_packet!
    send_packet(requested_size, payload); 
}

void test_api_narrowing() {
    long long large_offset = 70000; // Larger than short max (32767)

    // HIGH: Narrowing long long to short at API boundary
    // Silent truncation occurs, changing 70000 to an unexpected truncated short value (4464)
    int result = process_offset(large_offset);
    (void)result;
}

void test_api_char_truncation() {
    int status_code = 512; // 0x0200, lower byte is 0x00

    // HIGH: Truncation from int to unsigned char
    // Status code 512 gets truncated to 0, which makes it look like success (0) instead of error (512)
    set_flag(status_code);
}

// Dummy implementations to make compilation succeed
void send_packet(size_t packet_size, const char* data) {
    (void)packet_size;
    (void)data;
}

int process_offset(short offset) {
    return (int)offset;
}

void set_flag(unsigned char flag) {
    (void)flag;
}
