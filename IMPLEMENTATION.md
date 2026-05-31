# 💻 Implementation Details: Clang AST Analyzer Engine

This document outlines the low-level implementation details of the Implicit Conversion Hazard Analyzer, focusing on Clang AST structures, context traversal, deduplication, and reporting algorithms.

---

## 1. Clang AST Classes & Traversal

The core analyzer uses the standard LLVM/Clang LibTooling library to parse and traverse source files.

### Key Clang Classes Utilized:
*   **`clang::tooling::ClangTool`**: Coordinates parsing and executing the compilation database over source files.
*   **`clang::ASTConsumer`**: Triggered by the compiler frontend when a Translation Unit (TU) has been parsed. It forwards the AST to our visitor.
*   **`clang::RecursiveASTVisitor`**: Performs deep post-order traversal of the AST. We override **`VisitImplicitCastExpr(ImplicitCastExpr* S)`** to intercept all implicit type conversions.
*   **`clang::ImplicitCastExpr`**: The Clang AST node representing an implicit cast. We inspect this node using:
    *   `S->getCastKind()` to extract the type of conversion (e.g. `CK_IntegralCast`, `CK_FloatingToIntegral`).
    *   `S->getType()` to fetch the target type.
    *   `S->getSubExpr()->getType()` to fetch the source type.

---

## 2. Context-Aware Extraction Logic

When an `ImplicitCastExpr` is intercepted, we must determine its surrounding context. We implement this in **`ContextCollector`** by utilizing Clang's **`ParentMapContext`** to traverse up the AST tree.

### Core Context Checks:
1.  **Array Indexing**:
    *   We query parents of the cast expression.
    *   If a parent node is a **`clang::ArraySubscriptExpr`**, we verify if the cast expression is the index operand (not the array pointer). If true, it is flagged as an `ArrayIndex` context (+30 risk).
2.  **Loop Boundary**:
    *   We traverse up parents to find **`clang::ForStmt`**, **`clang::WhileStmt`**, or **`clang::DoStmt`**.
    *   We check if the cast occurs within the loop condition expression (e.g. `i < total`), flagged as a `LoopBound` context (+25 risk).
3.  **Comparison Operand**:
    *   If a parent is a **`clang::BinaryOperator`** with an operand kind of `BO_LT`, `BO_GT`, `BO_LE`, `BO_GE`, `BO_EQ`, or `BO_NE`, the cast is flagged as a `Comparison` context (+35 risk).
4.  **API/Function Boundary**:
    *   If a parent is a **`clang::CallExpr`** or **`clang::CXXConstructExpr`**, we match the cast expression to a function parameter, extracting the function's name and flagging it as an `APIBoundary` context (+30 risk).
5.  **Explicit Cast Suppression**:
    *   If any parent is a **`clang::ExplicitCastExpr`** (like a C-style cast or `static_cast`), this indicates the developer deliberately requested the conversion. We apply a major suppression factor (-40 risk), effectively dropping it below the threshold.

---

## 3. On-Visit Deduplication Engine

Clang parses header files (e.g. `#include <stdio.h>`, `#include "sqlite3.h"`) repeatedly for every single translation unit. This results in the same implicit conversion being visited hundreds of times, causing memory bloat and duplicate logs.

To solve this, we implemented an **on-visit deduplication engine** in `ImplicitConversionVisitor.cpp` using a lightweight hash map.

### Key Key Structure:
```cpp
struct FindingKey {
    std::string file;
    unsigned int line;
    unsigned int column;
    std::string source_type;
    std::string target_type;

    bool operator==(const FindingKey& other) const {
        return file == other.file && line == other.line &&
               column == other.column && source_type == other.source_type &&
               target_type == other.target_type;
    }
};
```
For every intercepted finding, we generate this key. If the key exists in our visited set, we discard the finding immediately. This guarantees **$O(1)$ lookup complexity** and ensures each finding is saved exactly once, resulting in massive performance improvements on huge codebases like SQLite.

---

## 4. Sorting & Capping Algorithm

Once the AST traversal finishes, findings are prepared for output.

### Steps:
1.  **Filtering**: Only findings with a calculated risk score strictly greater than or equal to `RiskThreshold` (default `80`) are retained.
2.  **Sorting**: The filtered collection is sorted in descending order using `std::sort` with a custom lambda comparing risk scores:
    ```cpp
    std::sort(findings.begin(), findings.end(), 
              [](const Finding& a, const Finding& b) {
                  return a.risk_score > b.risk_score;
              });
    ```
3.  **Truncation (The Top 15 Cap)**: When generating Markdown dashboards, the tool prints detailed blocks for only the top **15** findings. A trailing summary note lists how many findings were omitted to keep the dashboard high-signal, compact, and completely readable without bloating the filesystem.
