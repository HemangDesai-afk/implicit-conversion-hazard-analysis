#include "ImplicitConversionVisitor.h"
#include <clang/AST/Expr.h>
#include <clang/AST/PrettyPrinter.h>
#include <llvm/Support/raw_ostream.h>

#include <iostream>
#include <sstream>
#include <iomanip>

using namespace clang;

ImplicitConversionVisitor::ImplicitConversionVisitor(
    ASTContext& context,
    RiskScorer& scorer,
    FixItGenerator& fixer,
    int risk_threshold)
    : context_(context),
      scorer_(scorer),
      fixer_(fixer),
      risk_threshold_(risk_threshold) {}

bool ImplicitConversionVisitor::VisitImplicitCastExpr(ImplicitCastExpr* cast_expr) {
    total_conversions_++;

    // Get the source file manager
    const SourceManager& sm = context_.getSourceManager();
    SourceLocation loc = cast_expr->getBeginLoc();

    // Skip invalid locations
    if (loc.isInvalid() || loc.isMacroID()) {
        return true;
    }

    // Skip if in system headers (unless it's an API boundary we care about)
    if (sm.isInSystemHeader(loc)) {
        return true;
    }

    // Get type information
    QualType source_type = cast_expr->getSubExpr()->getType();
    QualType target_type = cast_expr->getType();
    CastKind kind = cast_expr->getCastKind();

    // Skip uninteresting conversions (e.g., identity, lvalue-to-rvalue on same type)
    if (kind == CastKind::CK_Dependent ||
        kind == CastKind::CK_NoOp ||
        kind == CastKind::CK_BitCast) {
        return true;
    }

    // Skip benign conversions that are always present and not dangerous
    if (kind == CastKind::CK_FunctionToPointerDecay ||
        kind == CastKind::CK_ArrayToPointerDecay ||
        kind == CastKind::CK_LValueToRValue ||
        kind == CastKind::CK_DerivedToBase ||
        kind == CastKind::CK_UncheckedDerivedToBase ||
        kind == CastKind::CK_ToVoid ||
        kind == CastKind::CK_NullToPointer) {
        return true;
    }

    // Skip boolean conversions unless in a risky context
    if (kind == CastKind::CK_IntegralToBoolean ||
        kind == CastKind::CK_FloatingToBoolean ||
        kind == CastKind::CK_PointerToBoolean) {
        // Still check — but they'll get low base score
    }

    // Collect context metadata
    ContextCollector collector(context_);

    // Get parent nodes via the AST context
    // We use a simplified approach: the parent is passed through traversal
    auto parents = context_.getParents(*cast_expr);

    const Stmt* parent = nullptr;
    if (!parents.empty()) {
        parent = parents[0].get<Stmt>();
    }

    ConversionContext ctx = collector.collect(cast_expr, parent, parents);

    // Compute risk score
    int base_risk = scorer_.baseRiskForConversionKind(
        getConversionKindString(kind),
        getTypeString(source_type),
        getTypeString(target_type));

    int context_risk = scorer_.score(ctx);

    // Combined score: base risk + context risk, clamped
    int total_score = base_risk + context_risk;
    if (total_score > 100) total_score = 100;
    if (total_score < 0) total_score = 0;

    // Build finding
    Finding finding;
    finding.file = sm.getFilename(loc).str();
    finding.line = sm.getSpellingLineNumber(loc);
    finding.column = sm.getSpellingColumnNumber(loc);
    finding.source_type = getTypeString(source_type);
    finding.target_type = getTypeString(target_type);
    finding.conversion_kind = getConversionKindString(kind);
    finding.risk_score = total_score;
    finding.risk_level = RiskScorer::levelFromScore(total_score);
    finding.context = ctx;
    finding.explanation = scorer_.explain(ctx, total_score);
    finding.fix_suggestion = fixer_.generateFix(finding);

    // Store all findings (filtering happens at output time)
    findings_.push_back(finding);

    return true;
}

void ImplicitConversionVisitor::printFindings() const {
    int count = 0;
    for (const auto& f : findings_) {
        if (!shouldReport(f)) continue;
        count++;

        llvm::errs() << "\n";
        llvm::errs() << "==========================================================\n";
        llvm::errs() << "  [" << RiskScorer::levelLabel(f.risk_level) << "] "
                     << "Score: " << f.risk_score << "/100\n";
        llvm::errs() << "==========================================================\n";
        llvm::errs() << "  File:     " << f.file << "\n";
        llvm::errs() << "  Location: " << f.line << ":" << f.column << "\n";
        llvm::errs() << "  From:     " << f.source_type << "\n";
        llvm::errs() << "  To:       " << f.target_type << "\n";
        llvm::errs() << "  Kind:     " << f.conversion_kind << "\n";
        llvm::errs() << "\n";
        llvm::errs() << "  " << f.explanation << "\n";
        llvm::errs() << "\n";
        llvm::errs() << "  Fix: " << f.fix_suggestion << "\n";
        llvm::errs() << "  Category: " << fixer_.fixCategory(f) << "\n";
    }

    if (count == 0) {
        llvm::errs() << "\nNo high-risk implicit conversions found "
                      << "(threshold: " << risk_threshold_ << ").\n";
    }
}

void ImplicitConversionVisitor::printSummary() const {
    int total = 0, low = 0, medium = 0, high = 0, critical = 0;
    for (const auto& f : findings_) {
        total++;
        switch (f.risk_level) {
            case RiskLevel::LOW:      low++; break;
            case RiskLevel::MEDIUM:   medium++; break;
            case RiskLevel::HIGH:     high++; break;
            case RiskLevel::CRITICAL: critical++; break;
        }
    }

    int reported = 0;
    for (const auto& f : findings_) {
        if (shouldReport(f)) reported++;
    }

    llvm::errs() << "\n";
    llvm::errs() << "==========================================================\n";
    llvm::errs() << "  SUMMARY\n";
    llvm::errs() << "==========================================================\n";
    llvm::errs() << "  Total implicit conversions found:  " << total << "\n";
    llvm::errs() << "  Total above threshold (reported):   " << reported << "\n";
    llvm::errs() << "  CRITICAL (75-100):  " << critical << "\n";
    llvm::errs() << "  HIGH     (50-74):   " << high << "\n";
    llvm::errs() << "  MEDIUM   (30-49):   " << medium << "\n";
    llvm::errs() << "  LOW      (0-29):    " << low << "\n";
    llvm::errs() << "==========================================================\n";
}

std::string ImplicitConversionVisitor::getSourceLocation(
    const SourceLocation& loc) const {
    const SourceManager& sm = context_.getSourceManager();
    if (loc.isInvalid()) return "unknown";
    return sm.getFilename(loc).str() + ":" +
           std::to_string(sm.getSpellingLineNumber(loc)) + ":" +
           std::to_string(sm.getSpellingColumnNumber(loc));
}

std::string ImplicitConversionVisitor::getTypeString(QualType type) const {
    PrintingPolicy policy(context_.getLangOpts());
    policy.SuppressScope = true;
    policy.SuppressUnwrittenScope = true;
    return type.getAsString(policy);
}

std::string ImplicitConversionVisitor::getConversionKindString(CastKind kind) const {
    switch (kind) {
        case CastKind::CK_IntegralCast:          return "IntegralCast";
        case CastKind::CK_IntegralToBoolean:     return "IntegralToBoolean";
        case CastKind::CK_IntegralToFloating:    return "IntegralToFloating";
        case CastKind::CK_IntegralToPointer:     return "IntegralToPointer";
        case CastKind::CK_FloatingCast:          return "FloatingCast";
        case CastKind::CK_FloatingToIntegral:    return "FloatingToIntegral";
        case CastKind::CK_FloatingToBoolean:     return "FloatingToBoolean";
        case CastKind::CK_PointerToIntegral:     return "PointerToIntegral";
        case CastKind::CK_PointerToBoolean:      return "PointerToBoolean";
        case CastKind::CK_BitCast:               return "BitCast";
        case CastKind::CK_LValueToRValue:        return "LValueToRValue";
        case CastKind::CK_ToUnion:               return "ToUnion";
        case CastKind::CK_ArrayToPointerDecay:   return "ArrayToPointerDecay";
        case CastKind::CK_FunctionToPointerDecay: return "FunctionToPointerDecay";
        case CastKind::CK_NullToPointer:         return "NullToPointer";
        case CastKind::CK_NoOp:                  return "NoOp";
        case CastKind::CK_Dependent:             return "Dependent";
        case CastKind::CK_UserDefinedConversion: return "UserDefinedConversion";
        case CastKind::CK_ConstructorConversion: return "ConstructorConversion";
        case CastKind::CK_VectorSplat:           return "VectorSplat";
        case CastKind::CK_BaseToDerived:         return "BaseToDerived";
        case CastKind::CK_DerivedToBase:         return "DerivedToBase";
        case CastKind::CK_UncheckedDerivedToBase: return "UncheckedDerivedToBase";
        case CastKind::CK_ToVoid:                return "ToVoid";
        case CastKind::CK_AtomicToNonAtomic:     return "AtomicToNonAtomic";
        case CastKind::CK_NonAtomicToAtomic:     return "NonAtomicToAtomic";
        default:                                 return "Other(" + std::to_string((int)kind) + ")";
    }
}

bool ImplicitConversionVisitor::shouldReport(const Finding& finding) const {
    return finding.risk_score >= risk_threshold_;
}
