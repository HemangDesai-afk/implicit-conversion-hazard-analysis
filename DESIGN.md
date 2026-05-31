# 📐 Design Document: Implicit Conversion Hazard Analyzer

## 1. Architectural Approach

The Implicit Conversion Hazard Analyzer is designed as a high-precision, low-noise static analysis tool leveraging Clang's front-end parsing and Abstract Syntax Tree (AST) infrastructure. 

Existing tools, such as `clang -Wconversion` or GCC's compiler warnings, treat all implicit conversions identically, leading to massive developer fatigue and warning ignore-rates. Our architectural thesis is that **the danger of an implicit conversion is defined by its data-flow and structural context**. A narrowing conversion inside a loop bound or an array index is a critical security risk (potentially leading to buffer overflows or infinite loops), while the same conversion inside a print/logging statement is negligible.

### Architectural Workflow

```
[C/C++ Source Files] 
         │
         ▼
    [ClangTool]  ◄─── [Compilation Database (compile_commands.json)]
         │
         ▼
[FrontendAction] ───► [AST Consumer] ───► [Recursive AST Visitor]
                                                 │
                                                 ▼
                                     [ImplicitCastExpr Visited]
                                                 │
                                                 ▼
                                        [Context Collector]
                                     (Identify AST Parent Node)
                                                 │
                                                 ▼
                                         [Risk Scorer]
                                    (Calculates 0-100 Score)
                                                 │
                                                 ▼
                                        [Deduplication Engine]
                                    (Location + Conversion Key)
                                                 │
                                                 ▼
                                       [Reporting Engine]
                                   (Sorted, Capped Markdown /
                                    Complete raw JSON/Console)
```

---

## 2. Context-Aware Risk Scoring Model

The risk score of an implicit conversion is calculated as:
$$\text{Risk Score} = \min(100, \text{Base Risk} + \sum \text{Context Weights})$$

### A. Base Risk (from Type Conversion Kind)
- **`FloatingToIntegral`** (+40): High risk due to severe truncation and potential precision loss.
- **`IntegralCast` (Signed ↔ Unsigned)** (+35): Sign-flipping is the classic root cause of buffer overflows and loop index bypasses.
- **`IntegralCast` (Larger → Smaller size)** (+25): Narrowing conversions lead to potential truncation and integer wrapping.
- **Pointer/Integer conversions** (+30): High risk for system portability.

### B. Contextual Weight Additions
- **Array Indexing Context** (+30): Array bounds accesses with implicit types are extreme security hazards.
- **Loop Boundary Context** (+25): Conversions in loop limits or loop step increments can cause infinite loops or off-by-one errors.
- **Comparison Context** (+35): Comparing different signs (e.g., signed `<` unsigned) is highly dangerous.
- **API Boundary Context** (+30): Passing an implicit type mismatch as an argument to a function signature.
- **Switch Case Enum Context** (+15): Switching on an enum implicitly converted to int can bypass compile-time case coverage checks.

### C. Mitigation / Suppression Factors (Negative Weights)
- **Developer Acknowledged** (-40): Implicit cast located inside an explicit cast expression (e.g., `(int)x` or `static_cast<int>(x)`).
- **Literal Prediction** (-10): If the source is an integer literal/constant that fits safely in the target range (e.g., `char c = 42;`).

---

## 3. Alternative Approaches Considered

During the design phase, three alternative architectures were evaluated:

### Alternative 1: Textual Regex Pattern Matching (grep-based)
*   **Pros**: Fast execution, zero dependencies on compiler tools.
*   **Cons**: Extremely high false-positive rate. Cannot resolve type definitions, macro expansions, structure member types, or template instantiations. Totally unviable for C/C++ source code.

### Alternative 2: Abstract Interpretation & Symbolic Execution (e.g., Clang Static Analyzer / Path-Sensitive Analysis)
*   **Pros**: Tracks values across functions, resolves run-time constraints, and mathematically proves correctness.
*   **Cons**: Extremely high performance overhead, leading to minutes or hours of compilation time on large OSS databases (FFmpeg). High false-negative rate when symbolic solvers time out.

### Alternative 3: Clang-Tidy plugin (AST Matchers)
*   **Pros**: Native integration into the IDE and compilation database pipeline.
*   **Cons**: AST Matchers are hard to write for complex, multi-level parent matching (e.g. searching deep nested loops for loop conditions or looking up variable definitions across system files).

### Chosen Approach: Recursive AST Visitor with Dynamic Context Collection
By using Clang's `RecursiveASTVisitor` combined with a custom **`ContextCollector`** (which walks up the AST parent hierarchy via `clang::ParentMapContext`), we achieved the best of both worlds:
1.  **Compiler Accuracy**: 100% accurate type resolution, macro expansions, and structural understanding via Clang.
2.  **High Performance**: Linear traversal of the AST in a single pass.
3.  **Algorithmic Flexibility**: Easy to write custom C++ code to inspect arbitrary AST parent nodes, calculate dynamic scores, and generate specific Fix-It suggestions.
