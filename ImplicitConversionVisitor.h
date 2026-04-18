#ifndef IMPLICIT_CONVERSION_VISITOR_H
#define IMPLICIT_CONVERSION_VISITOR_H

#include "ContextCollector.h"
#include "RiskScorer.h"
#include "FixItGenerator.h"

#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/Expr.h>
#include <clang/Frontend/CompilerInstance.h>

#include <vector>
#include <string>

/// AST visitor that catalogs all implicit conversions with context and risk
class ImplicitConversionVisitor
    : public clang::RecursiveASTVisitor<ImplicitConversionVisitor> {
public:
    ImplicitConversionVisitor(clang::ASTContext& context,
                               RiskScorer& scorer,
                               FixItGenerator& fixer,
                               int risk_threshold = 50);

    /// Visit ImplicitCastExpr nodes — the primary implicit conversion AST node
    bool VisitImplicitCastExpr(clang::ImplicitCastExpr* cast_expr);

    /// Get all findings after traversal
    const std::vector<Finding>& getFindings() const { return findings_; }

    /// Print all findings to stdout in a human-readable format
    void printFindings() const;

    /// Print a summary (counts by risk level)
    void printSummary() const;

private:
    clang::ASTContext& context_;
    RiskScorer& scorer_;
    FixItGenerator& fixer_;
    int risk_threshold_;

    std::vector<Finding> findings_;

    /// Total conversions seen (including low-risk ones we don't report)
    int total_conversions_ = 0;

    /// Get the source location as a string
    std::string getSourceLocation(const clang::SourceLocation& loc) const;

    /// Get a human-readable type string
    std::string getTypeString(clang::QualType type) const;

    /// Get the conversion kind as a string
    std::string getConversionKindString(clang::CastKind kind) const;

    /// Should this finding be reported (above threshold)?
    bool shouldReport(const Finding& finding) const;
};

#endif // IMPLICIT_CONVERSION_VISITOR_H
