#include "ContextCollector.h"
#include <clang/AST/Expr.h>
#include <clang/AST/ParentMapContext.h>
#include <clang/AST/ExprCXX.h>
#include <clang/Basic/SourceManager.h>
#include <sstream>

using namespace clang;

ContextCollector::ContextCollector(ASTContext& context)
    : ast_context_(context) {}

ConversionContext ContextCollector::collect(const ImplicitCastExpr* cast_expr,
                                              const Stmt* parent,
                                              DynTypedNodeList parents) {
    ConversionContext ctx;
    ctx.type = determineContextType(parent, parents);
    ctx.parent_expr = parent;
    ctx.is_sign_mismatch = isSignMismatch(cast_expr, parent);
    ctx.comparison_other_type = getComparisonOtherType(cast_expr, parent);
    ctx.inside_explicit_cast = isInsideExplicitCast(parents);
    ctx.nesting_depth = computeNestingDepth(parents);
    ctx.is_literal = isLiteral(cast_expr);
    ctx.is_api_boundary = isApiBoundary(parent);

    // Extract loop variable if in a loop context
    if (ctx.type == ConversionContextType::LOOP_BOUND ||
        ctx.type == ConversionContextType::LOOP_INIT) {
        if (const auto* decl_ref = dyn_cast_or_null<DeclRefExpr>(
                cast_expr->getSubExpr()->IgnoreImpCasts())) {
            if (const auto* var_decl = dyn_cast<VarDecl>(decl_ref->getDecl())) {
                ctx.loop_variable = var_decl->getNameAsString();
            }
        }
    }

    // Extract function name if in a call argument context
    if (ctx.type == ConversionContextType::FUNCTION_ARGUMENT) {
        ctx.function_name = extractFunctionName(parent);
    }

    // Build a brief surrounding context string
    std::ostringstream oss;
    if (ctx.is_sign_mismatch) {
        oss << "sign-mismatch comparison";
    }
    if (ctx.inside_explicit_cast) {
        if (!oss.str().empty()) oss << ", ";
        oss << "inside explicit cast";
    }
    ctx.surrounding_context = oss.str();

    return ctx;
}

ConversionContextType ContextCollector::determineContextType(
    const Stmt* parent, DynTypedNodeList parents) {

    if (!parent) return ConversionContextType::UNKNOWN;

    // Check for array subscript
    if (isa<ArraySubscriptExpr>(parent)) {
        return ConversionContextType::ARRAY_INDEX;
    }

    // Check for comparisons
    if (const auto* bin_op = dyn_cast<BinaryOperator>(parent)) {
        if (bin_op->isComparisonOp() || bin_op->isRelationalOp()) {
            return ConversionContextType::COMPARISON_OPERAND;
        }
        if (bin_op->isAdditiveOp() || bin_op->isMultiplicativeOp() ||
            bin_op->isShiftOp()) {
            return ConversionContextType::ARITHMETIC_OPERAND;
        }
    }

    // Check for call expression (function argument)
    if (isa<CallExpr>(parent)) {
        return ConversionContextType::FUNCTION_ARGUMENT;
    }

    // Check for C++ member call
    if (isa<CXXMemberCallExpr>(parent) || isa<CXXOperatorCallExpr>(parent)) {
        return ConversionContextType::FUNCTION_ARGUMENT;
    }

    // Check for return statement
    if (isa<ReturnStmt>(parent)) {
        return ConversionContextType::RETURN_VALUE;
    }

    // Check for assignment
    if (const auto* bin_op = dyn_cast<BinaryOperator>(parent)) {
        if (bin_op->isAssignmentOp()) {
            return ConversionContextType::ASSIGNMENT_RHS;
        }
    }

    // Check for loop contexts — need to look further up the parent chain
    for (const auto& node : parents) {
        if (const auto* for_stmt = node.get<ForStmt>()) {
            // Determine if this is the loop bound (condition) or init
            if (for_stmt->getCond() == parent) {
                return ConversionContextType::LOOP_BOUND;
            }
            if (for_stmt->getInc() == parent) {
                return ConversionContextType::LOOP_BOUND;
            }
            if (for_stmt->getInit() == parent) {
                return ConversionContextType::LOOP_INIT;
            }
            // Check if parent is the body and this is inside
            if (for_stmt->getBody() == parent) {
                return ConversionContextType::LOOP_BOUND;
            }
        }

        if (const auto* while_stmt = node.get<WhileStmt>()) {
            if (while_stmt->getCond() == parent ||
                while_stmt->getBody() == parent) {
                return ConversionContextType::LOOP_BOUND;
            }
        }

        if (const auto* do_stmt = node.get<DoStmt>()) {
            if (do_stmt->getCond() == parent ||
                do_stmt->getBody() == parent) {
                return ConversionContextType::LOOP_BOUND;
            }
        }

        if (const auto* range_for = node.get<CXXForRangeStmt>()) {
            if (range_for->getRangeInit() == parent) {
                return ConversionContextType::LOOP_BOUND;
            }
        }
    }

    // Check for switch condition
    if (const auto* switch_stmt = dyn_cast<SwitchStmt>(parent)) {
        return ConversionContextType::SWITCH_CONDITION;
    }
    // Also check parent chain for switch
    for (const auto& node : parents) {
        if (node.get<SwitchStmt>()) {
            return ConversionContextType::SWITCH_CONDITION;
        }
    }

    // Check for if condition
    if (isa<IfStmt>(parent)) {
        return ConversionContextType::IF_CONDITION;
    }
    for (const auto& node : parents) {
        if (node.get<IfStmt>()) {
            return ConversionContextType::IF_CONDITION;
        }
    }

    // Check for explicit cast wrapper
    if (isa<ExplicitCastExpr>(parent)) {
        return ConversionContextType::CAST_EXPRESSION;
    }

    // Check for sizeof (low risk)
    if (isa<UnaryExprOrTypeTraitExpr>(parent)) {
        return ConversionContextType::SIZEOF_ARGUMENT;
    }

    // Check for paren expression (neutral)
    if (isa<ParenExpr>(parent)) {
        return ConversionContextType::PARENTHESIS;
    }

    return ConversionContextType::UNKNOWN;
}

std::string ContextCollector::extractFunctionName(const Stmt* parent) {
    if (!parent) return "";

    if (const auto* call = dyn_cast<CallExpr>(parent)) {
        const Expr* callee = call->getCallee();
        if (const auto* decl_ref = dyn_cast<DeclRefExpr>(callee->IgnoreImpCasts())) {
            return decl_ref->getDecl()->getNameAsString();
        }
        if (const auto* member = dyn_cast<MemberExpr>(callee->IgnoreImpCasts())) {
            return member->getMemberDecl()->getNameAsString();
        }
    }

    if (const auto* member_call = dyn_cast<CXXMemberCallExpr>(parent)) {
        if (const auto* member = member_call->getMethodDecl()) {
            return member->getNameAsString();
        }
    }

    return "unknown_function";
}

bool ContextCollector::isSignMismatch(const ImplicitCastExpr* cast_expr,
                                       const Stmt* parent) {
    if (!parent) return false;

    const auto* bin_op = dyn_cast<BinaryOperator>(parent);
    if (!bin_op) return false;
    if (!bin_op->isComparisonOp() && !bin_op->isRelationalOp()) return false;

    // Get the source type of the cast
    QualType source_type = cast_expr->getSubExpr()->getType();
    QualType target_type = cast_expr->getType();

    // Check for signed/unsigned mismatch
    bool source_signed = source_type->isSignedIntegerType();
    bool source_unsigned = source_type->isUnsignedIntegerType();
    bool target_signed = target_type->isSignedIntegerType();
    bool target_unsigned = target_type->isUnsignedIntegerType();

    // The other operand
    const Expr* other = nullptr;
    if (bin_op->getLHS() == cast_expr) {
        other = bin_op->getRHS();
    } else if (bin_op->getRHS() == cast_expr) {
        other = bin_op->getLHS();
    }

    if (!other) return false;

    QualType other_type = other->getType();

    // Sign mismatch: one is signed, the other is unsigned
    return (source_signed && other_type->isUnsignedIntegerType()) ||
           (source_unsigned && other_type->isSignedIntegerType());
}

std::string ContextCollector::getComparisonOtherType(
    const ImplicitCastExpr* cast_expr, const Stmt* parent) {

    if (!parent) return "";

    const auto* bin_op = dyn_cast<BinaryOperator>(parent);
    if (!bin_op) return "";
    if (!bin_op->isComparisonOp() && !bin_op->isRelationalOp()) return "";

    const Expr* other = nullptr;
    if (bin_op->getLHS() == cast_expr) {
        other = bin_op->getRHS();
    } else if (bin_op->getRHS() == cast_expr) {
        other = bin_op->getLHS();
    }

    if (!other) return "";
    return other->getType().getAsString();
}

int ContextCollector::computeNestingDepth(DynTypedNodeList parents) {
    int depth = 0;
    for (const auto& node : parents) {
        if (node.get<CompoundStmt>() || node.get<ForStmt>() ||
            node.get<WhileStmt>() || node.get<DoStmt>() ||
            node.get<IfStmt>() || node.get<SwitchStmt>()) {
            depth++;
        }
    }
    return depth;
}

bool ContextCollector::isInsideExplicitCast(DynTypedNodeList parents) {
    for (const auto& node : parents) {
        if (node.get<ExplicitCastExpr>()) {
            return true;
        }
    }
    return false;
}

bool ContextCollector::isApiBoundary(const Stmt* stmt) {
    if (!stmt) return false;
    // Check if the source location is in a system header
    SourceLocation loc = stmt->getBeginLoc();
    if (loc.isValid()) {
        const SourceManager& sm = ast_context_.getSourceManager();
        return sm.isInSystemHeader(loc);
    }
    return false;
}

bool ContextCollector::isLiteral(const ImplicitCastExpr* cast_expr) {
    if (!cast_expr) return false;
    const Expr* sub = cast_expr->getSubExpr();
    if (!sub) return false;
    // Strip implicit casts to find the root
    sub = sub->IgnoreImpCasts();
    return isa<IntegerLiteral>(sub) || isa<FloatingLiteral>(sub) ||
           isa<CharacterLiteral>(sub) || isa<StringLiteral>(sub) ||
           isa<CXXBoolLiteralExpr>(sub);
}
