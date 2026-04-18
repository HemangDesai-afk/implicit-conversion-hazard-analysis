# Implicit Conversion Hazard Analyzer

A Clang AST-based static analysis tool that identifies dangerous implicit type
conversions in C/C++ code. Unlike `-Wconversion` (which is too noisy) or
`-Wsign-compare` (which is too narrow), this tool categorizes implicit conversions
by **actual risk level** using data-flow context.

## The Problem

C/C++ implicit conversions cause a class of bugs that compilers warn about
inconsistently. A narrowing conversion in a loop bound is far more dangerous
than one in a logging statement — but no existing tool makes this distinction.

Common dangerous patterns:
- **Sign mismatch in comparisons**: `int i = -1; if (i < size_t_val)` — always true
- **Float-to-int in loop bounds**: `for (int i = 0; i < double_val; i++)` — truncation
- **Narrowing in function arguments**: `take_short(long_long_val)` — silent truncation
- **Enum-to-int in switch**: missing case coverage silently accepted

## How It Works

### Architecture

```
Source Code → ClangTool → AST Traversal → ImplicitCastExpr Detection
                                              ↓
                                    Context Collection
                                    (loop bound? array index?
                                     comparison? API boundary?)
                                              ↓
                                    Risk Scoring (0-100)
                                    (base risk + context weights)
                                              ↓
                                    Fix-It Generation
                                    (static_cast? type change?
                                     sign fix? loop fix?)
                                              ↓
                                    Filtered Report
                                    (only above threshold)
```

### Risk Scoring Model

Each implicit conversion receives a score from 0–100 based on:

| Context | Weight | Example |
|---------|--------|---------|
| Array index operand | +30 | `arr[negative_int]` → OOB |
| Loop bound / iteration | +25 | `for (int i = 0; i < size_t; i++)` |
| Sign-mismatch comparison | +35 | `int < size_t` where int is negative |
| Function argument at API boundary | +30 | `syscall(int_param)` where API expects `size_t` |
| Switch condition (enum-to-int) | +15 | `switch (enum_val)` with missing cases |
| Arithmetic operand | +15 | `int * double` → truncation |
| Assignment to smaller type | +10 | `char = long_long` |
| Logging/printing | +2 | `printf("%d", double)` |
| Inside explicit cast | −40 | `static_cast<T>(expr)` — developer acknowledged |
| Literal source | −10 | `char x = 42` — constant, predictable |

Base risk from conversion kind:
- `float`/`double` → `int`: +40 (truncation)
- Signed ↔ Unsigned: +35 (sign flip)
- Larger → Smaller integer: +25 (narrowing)
- Pointer ↔ Integer: +30 (portability)

### Fix-It Suggestions

For each high-risk conversion, the tool suggests:
1. **`static_cast<T>`**: Makes the conversion explicit and visible
2. **Type change**: Change the variable/parameter type to match
3. **Sign fix**: For signed/unsigned comparisons, specific guidance
4. **Loop fix**: For loop bounds, correct iteration patterns
5. **Scoped enum**: For enum-to-int, use `enum class`

## Building

### Prerequisites
- Clang 17+ development libraries (with libclang-cpp)
- CMake 3.13+
- C++17 compiler

### Build
```bash
./scripts/build.sh
# or manually:
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang
cmake --build build -j$(nproc)
```

## Usage

### Analyze a single file (no compile database needed)
```bash
./build/implicit-conversion-hazard --risk-threshold 30 myfile.c
```

### Analyze with compile_commands.json
```bash
# Generate compile_commands.json first
bear -- make   # or cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=1

# Run analyzer
./build/implicit-conversion-hazard -p /path/to/build/dir \
    --risk-threshold 50 -- \
    /path/to/source/file.c
```

### Analyze all files in a project
```bash
./build/implicit-conversion-hazard -p /path/to/build/dir \
    --risk-threshold 50 -- \
    file1.c file2.cpp file3.c ...
```

### Output modes
```bash
# Default: human-readable with findings + summary
./build/implicit-conversion-hazard --risk-threshold 50 file.c

# Summary only (just counts)
./build/implicit-conversion-hazard --summary-only file.c

# JSON output (for automated processing)
./build/implicit-conversion-hazard --json file.c

# Show all conversions including low-risk
./build/implicit-conversion-hazard --show-all file.c
```

### Command-line options
| Option | Description | Default |
|--------|-------------|---------|
| `--risk-threshold N` | Minimum risk score to report (0–100) | 50 |
| `--show-all` | Report all conversions, regardless of risk | off |
| `--summary-only` | Only print summary statistics | off |
| `--json` | Output findings as JSON | off |
| `-p DIR` | Build directory with compile_commands.json | . |
| `--extra-arg=FLAG` | Additional compiler flag (e.g., `-xc++`) | — |

## Test Results

### Test: Narrowing Conversions (`test/test-narrowing.c`)
```
Total implicit conversions: 7
CRITICAL: 0  HIGH: 2  MEDIUM: 4  LOW: 1

Key findings:
- int → unsigned int (sign flip in function argument): HIGH 50/100
- unsigned int → unsigned short (narrowing in function argument): HIGH 55/100
```

### Test: Sign Comparison (`test/test-sign-compare.c`)
```
Total implicit conversions: 13
CRITICAL: 1  HIGH: 5  MEDIUM: 5  LOW: 2

Key findings:
- int → size_t in comparison (classic -1 bug): CRITICAL 80/100
- int → unsigned int in loop condition: HIGH 70/100
- unsigned int loop wrap-around (i >= 0 always true): HIGH 70/100
```

### Test: Float-to-Int (`test/test-float-loop.c`)
```
Total implicit conversions: 7
CRITICAL: 0  HIGH: 3  MEDIUM: 4  LOW: 0

Key findings:
- double → int in return value: HIGH 65/100
- double → int truncation in assignment: HIGH 55/100
```

### Test: Enum Conversions (`test/test-enum-switch.c`)
```
Total implicit conversions: 17
CRITICAL: 0  HIGH: 6  MEDIUM: 2  LOW: 9

Key findings:
- enum Color → int in arithmetic: HIGH 60/100
- Different enum types in comparison: HIGH 65/100
- Enum in switch condition (missing case): MEDIUM 35/100
```

## Evaluation (Phases 8–9)

### Planned Analysis Targets
1. **SQLite** — known integer-related CVEs (CVE-2015-7036)
2. **LibPNG** — integer overflow CVEs (CVE-2015-8126, CVE-2016-10087)
3. **cURL** or **OpenSSL** — complex C with API boundaries

### False-Positive Rate Comparison
```bash
# Compare against clang -Wconversion
./evaluation/compare_wconversion.sh test/test-sign-compare.c

# Correlate findings with known CVEs
./evaluation/run_on_project.sh /path/to/sqlite --threshold 50
python3 evaluation/cve_correlation.py output/sqlite_findings.json cves/sqlite_cves.json
```

## Portability & Sharing

This project is designed to be portable across Linux distributions (Fedora, Ubuntu, WSL). However, please note:

- **Do NOT share the `build/` directory**: This folder contains absolute paths and binary files specific to your machine. Your friend should create their own `build/` directory.
- **Do NOT share `compile_commands.json`**: This file contains absolute paths to the source files on your specific system. It should be regenerated on each new machine.
- **Portability Logic**: The `CMakeLists.txt` is configured to automatically detect LLVM installations and system headers on both Fedora and Ubuntu/WSL.

To share with a friend, only send the source files and `CMakeLists.txt` (or use the `.gitignore` provided).

## Comparison with clang -Wconversion

| Metric | `-Wconversion` | This Tool |
|--------|---------------|-----------|
| Total warnings | High (noisy) | Filtered by risk |
| Context awareness | None | Full AST context |
| Sign mismatch detection | Partial (-Wsign-compare) | Complete |
| Fix-It suggestions | Some | Detailed, categorized |
| FP rate | ~60–80% | Target: <30% |

## File Structure

```
implicit-conversion-hazard/
├── CMakeLists.txt                 # Build system
├── main.cpp                       # Entry point + CLI
├── ImplicitConversionVisitor.h/.cpp  # AST visitor
├── ContextCollector.h/.cpp        # Context metadata extraction
├── RiskScorer.h/.cpp              # Risk scoring model
├── FixItGenerator.h/.cpp          # Fix-It suggestions
├── SimpleCompilationDB.h          # Fallback compilation database
├── compile_commands.json          # For test files
├── test/
│   ├── test-narrowing.c           # Narrowing conversion tests
│   ├── test-sign-compare.c        # Sign mismatch tests
│   ├── test-float-loop.c          # Float-to-int tests
│   └── test-enum-switch.c         # Enum conversion tests
├── evaluation/
│   ├── run_on_project.sh          # Run on OSS project
│   ├── compare_wconversion.sh     # FP rate comparison
│   └── cve_correlation.py         # CVE cross-referencing
└── scripts/
    └── build.sh                   # Build wrapper
```

## Limitations

- **No interprocedural analysis**: Doesn't track types across function boundaries
- **No value-set analysis**: Doesn't determine if a variable's range fits the target type
- **Single-TU only**: Analyzes one translation unit at a time
- **Requires Clang AST**: GCC and MSVC code paths not supported

## License

MIT
