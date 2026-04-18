#ifndef CONTEXT_COLLECTOR_H
#define CONTEXT_COLLECTOR_H

#include <clang/AST/Expr.h>
#include <clang/AST/ParentMapContext.h>
#include <string>
#include <vector>

/// Types of contexts in which an implicit conversion can appear
enum class ConversionContextType {
    UNKNOWN,
    ARRAY_INDEX,          // Conversion in array/subscript operand
    LOOP_BOUND,           // Conversion in loop condition or iteration variable
    LOOP_INIT,            // Conversion in loop initializer
    COMPARISON_OPERAND,   // Conversion as operand of ==, <, >, <=, >=, !=
    FUNCTION_ARGUMENT,    // Conversion as argument to a function call
    RETURN_VALUE,         // Conversion in a return statement
    ASSIGNMENT_RHS,       // Conversion on RHS of an assignment
    ARITHMETIC_OPERAND,   // Conversion as operand of +, -, *, /, %
    SWITCH_CONDITION,     // Conversion in switch condition (enum-to-int)
    IF_CONDITION,         // Conversion in if condition
    CAST_EXPRESSION,      // Conversion inside an explicit cast (lower risk)
    LOGGING,              // Conversion in printf/logging context
    SIZEOF_ARGUMENT,      // Conversion inside sizeof (low risk)
    PARENTHESIS           // Conversion in a paren expression
};

/// Metadata about the context surrounding an implicit conversion
struct ConversionContext {
    ConversionContextType type = ConversionContextType::UNKNOWN;

    // The parent expression that contains this conversion
    const clang::Stmt* parent_expr = nullptr;

    // For array index: the array being indexed
    std::string array_name;

    // For function argument: the function name and parameter position
    std::string function_name;
    int argument_position = -1;

    // For comparison: the other operand's type (to detect sign mismatch)
    std::string comparison_other_type;
    bool is_sign_mismatch = false;

    // For loop: the loop variable name
    std::string loop_variable;

    // Whether this conversion is inside an explicit cast
    bool inside_explicit_cast = false;

    // Whether this is in a system/API header (higher risk)
    bool is_api_boundary = false;

    // Depth of nesting (deeper = harder to reason about)
    int nesting_depth = 0;

    // Whether the source expression is a literal (lower risk)
    bool is_literal = false;

    // Description of the surrounding code context
    std::string surrounding_context;
};

/// Collects context metadata for an implicit conversion at a given AST node
class ContextCollector {
public:
    ContextCollector(clang::ASTContext& context);

    /// Analyze the context of an ImplicitCastExpr and populate metadata
    ConversionContext collect(const clang::ImplicitCastExpr* cast_expr,
                               const clang::Stmt* parent,
                               clang::DynTypedNodeList parents);

private:
    clang::ASTContext& ast_context_;

    /// Determine the context type from the parent AST node
    ConversionContextType determineContextType(const clang::Stmt* parent,
                                                 clang::DynTypedNodeList parents);

    /// Extract the function name if this is a call argument
    std::string extractFunctionName(const clang::Stmt* parent);

    /// Check if this is a sign-mismatch comparison
    bool isSignMismatch(const clang::ImplicitCastExpr* cast_expr,
                         const clang::Stmt* parent);

    /// Get the type string of the "other" operand in a comparison
    std::string getComparisonOtherType(const clang::ImplicitCastExpr* cast_expr,
                                        const clang::Stmt* parent);

    /// Estimate nesting depth from parent chain
    int computeNestingDepth(clang::DynTypedNodeList parents);

    /// Check if the conversion is inside an explicit cast
    bool isInsideExplicitCast(clang::DynTypedNodeList parents);

    /// Check if this is at an API boundary (system header involvement)
    bool isApiBoundary(const clang::Stmt* stmt);

    /// Check if source is a literal
    bool isLiteral(const clang::ImplicitCastExpr* cast_expr);
};

#endif // CONTEXT_COLLECTOR_H
