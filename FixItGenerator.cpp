#include "FixItGenerator.h"
#include <sstream>

FixItGenerator::FixItGenerator() {}

std::string FixItGenerator::generateFix(const Finding& finding) const {
    switch (finding.context.type) {
        case ConversionContextType::ARRAY_INDEX:
            return suggestStaticCast(finding);

        case ConversionContextType::LOOP_BOUND:
            return suggestLoopFix(finding);

        case ConversionContextType::COMPARISON_OPERAND:
            if (finding.context.is_sign_mismatch) {
                return suggestSignFix(finding);
            }
            return suggestStaticCast(finding);

        case ConversionContextType::FUNCTION_ARGUMENT:
            if (finding.context.is_api_boundary) {
                return suggestTypeChange(finding);
            }
            return suggestStaticCast(finding);

        case ConversionContextType::ASSIGNMENT_RHS:
            return suggestTypeChange(finding);

        case ConversionContextType::ARITHMETIC_OPERAND:
            return suggestWiderType(finding);

        case ConversionContextType::SWITCH_CONDITION:
            return "Consider using enum class (scoped enum) to prevent implicit conversion, "
                   "or add explicit cast to document intent";

        case ConversionContextType::RETURN_VALUE:
            return suggestTypeChange(finding);

        case ConversionContextType::CAST_EXPRESSION:
            return "An explicit cast is already present — verify the cast target type is correct";

        default:
            return suggestStaticCast(finding);
    }
}

std::string FixItGenerator::generateReplacement(const Finding& finding) const {
    // For machine-applicable fix-it: wrap the expression in static_cast<TargetType>(...)
    std::ostringstream oss;
    oss << "static_cast<" << finding.target_type << ">(/* expression */)";
    return oss.str();
}

std::string FixItGenerator::fixCategory(const Finding& finding) const {
    switch (finding.context.type) {
        case ConversionContextType::ARRAY_INDEX:
            return "add_explicit_cast";
        case ConversionContextType::LOOP_BOUND:
            return "fix_loop_bound_type";
        case ConversionContextType::COMPARISON_OPERAND:
            if (finding.context.is_sign_mismatch) {
                return "fix_sign_mismatch";
            }
            return "add_explicit_cast";
        case ConversionContextType::FUNCTION_ARGUMENT:
            return "add_explicit_cast";
        case ConversionContextType::ASSIGNMENT_RHS:
            return "change_variable_type";
        case ConversionContextType::ARITHMETIC_OPERAND:
            return "use_wider_type";
        case ConversionContextType::SWITCH_CONDITION:
            return "use_scoped_enum";
        case ConversionContextType::RETURN_VALUE:
            return "change_return_type";
        case ConversionContextType::CAST_EXPRESSION:
            return "verify_cast";
        default:
            return "add_explicit_cast";
    }
}

std::string FixItGenerator::suggestStaticCast(const Finding& finding) const {
    std::ostringstream oss;
    oss << "Add explicit cast to document intent: "
        << "static_cast<" << finding.target_type << ">(expression). "
        << "This makes the conversion visible to reviewers and suppresses "
        << "-Wconversion if the cast is deliberate.";
    return oss.str();
}

std::string FixItGenerator::suggestTypeChange(const Finding& finding) const {
    std::ostringstream oss;
    oss << "Consider changing the destination variable/parameter type from "
        << finding.target_type << " to " << finding.source_type
        << " to eliminate the implicit conversion entirely. "
        << "If the destination type cannot be changed (e.g., external API), "
        << "add an explicit static_cast with a comment explaining why the "
        << "conversion is safe in this context.";
    return oss.str();
}

std::string FixItGenerator::suggestWiderType(const Finding& finding) const {
    std::ostringstream oss;
    oss << "Consider using a wider type for the arithmetic expression to "
        << "prevent silent truncation. For example, use " << finding.source_type
        << " or a signed variant if negative values are possible. "
        << "Alternatively, wrap the operands in explicit casts to the "
        << "desired result type.";
    return oss.str();
}

std::string FixItGenerator::suggestSignFix(const Finding& finding) const {
    std::ostringstream oss;
    oss << "Signed/unsigned comparison mismatch detected. Options:\n"
        << "  1. Cast the unsigned operand to signed if you know it is always "
           "non-negative: static_cast<" << finding.source_type << ">(unsigned_value)\n"
        << "  2. Change the signed variable to unsigned if it should never be negative\n"
        << "  3. Use a common wider signed type for both operands\n"
        << "  4. Add an assertion: assert(signed_value >= 0) before the comparison";

    if (!finding.context.comparison_other_type.empty()) {
        oss << "\n  Other operand type: " << finding.context.comparison_other_type;
    }

    return oss.str();
}

std::string FixItGenerator::suggestLoopFix(const Finding& finding) const {
    std::ostringstream oss;
    oss << "Narrowing in a loop bound can cause incorrect iteration count "
        << "(infinite loop or early termination). Fix options:\n"
        << "  1. Make the loop variable match the bound type:\n"
        << "     for (" << finding.source_type << " i = 0; i < bound; ++i)\n"
        << "  2. Cast the bound explicitly if you know it fits:\n"
        << "     for (int i = 0; i < static_cast<int>(bound); ++i)\n"
        << "  3. Add a runtime check/assertion that the bound fits the loop variable type";
    return oss.str();
}
