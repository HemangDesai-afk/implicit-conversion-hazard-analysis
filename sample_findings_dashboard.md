# Implicit Conversion Hazard Dashboard

## Executive Summary

| Severity | Count | Color |
| :--- | :--- | :--- |
| **CRITICAL** | 4 | 🔴 |
| **HIGH** | 0 | 🟠 |
| **MEDIUM** | 0 | 🟡 |
| **LOW** | 0 | 🔵 |

## Findings Detail (Top Critical & High Risk)

### 🔴 CRITICAL (80/100)

**Location**: `OSS-Codebases/sqlite/src/alter.c:57:7`  
**Conversion**: `int` → `unsigned long` (`IntegralCast`)

> CRITICAL (80/100): signed/unsigned comparison mismatch — classic bug pattern

```c
  if( iMin>=sizeof(zBuf) ) iMin = sizeof(zBuf)-1;
      ^
```

**Fix**: Signed/unsigned comparison mismatch detected. Options:
  1. Cast the unsigned operand to signed if you know it is always non-negative: static_cast<int>(unsigned_value)
  2. Change the signed variable to unsigned if it should never be negative
  3. Use a common wider signed type for both operands
  4. Add an assertion: assert(signed_value >= 0) before the comparison
  Other operand type: unsigned long

---

### 🔴 CRITICAL (80/100)

**Location**: `OSS-Codebases/sqlite/src/analyze.c:60:14`  
**Conversion**: `int` → `unsigned long` (`IntegralCast`)

> CRITICAL (80/100): signed/unsigned comparison mismatch — classic bug pattern

```c
  if( iMax>=sizeof(zBuf) ) iMax = sizeof(zBuf)-1;
      ^
```

**Fix**: Signed/unsigned comparison mismatch detected. Options:
  1. Cast the unsigned operand to signed if you know it is always non-negative: static_cast<int>(unsigned_value)
  2. Change the signed variable to unsigned if it should never be negative
  3. Use a common wider signed type for both operands
  4. Add an assertion: assert(signed_value >= 0) before the comparison
  Other operand type: unsigned long

---

### 🔴 CRITICAL (80/100)

**Location**: `OSS-Codebases/openssl/crypto/bio/bf_lbuf.c:203:13`  
**Conversion**: `int` → `size_t` (`IntegralCast`)

> CRITICAL (80/100): signed/unsigned comparison mismatch — classic bug pattern

```c
  if (amt > len) {
      ^
```

**Fix**: Signed/unsigned comparison mismatch detected. Options:
  1. Cast the unsigned operand to signed if you know it is always non-negative: static_cast<int>(unsigned_value)
  2. Change the signed variable to unsigned if it should never be negative
  3. Use a common wider signed type for both operands
  4. Add an assertion: assert(signed_value >= 0) before the comparison
  Other operand type: size_t

---

### 🔴 CRITICAL (80/100)

**Location**: `OSS-Codebases/ffmpeg/libavcodec/put_str.c:84:18`  
**Conversion**: `int` → `unsigned int` (`IntegralCast`)

> CRITICAL (80/100): signed/unsigned comparison mismatch — classic bug pattern

```c
  if (size > max_size) {
      ^
```

**Fix**: Signed/unsigned comparison mismatch detected. Options:
  1. Cast the unsigned operand to signed if you know it is always non-negative: static_cast<int>(unsigned_value)
  2. Change the signed variable to unsigned if it should never be negative
  3. Use a common wider signed type for both operands
  4. Add an assertion: assert(signed_value >= 0) before the comparison
  Other operand type: unsigned int

---

*... and 0 more findings omitted to keep report concise.*
