#include "RiskScorer.h"
#include <sstream>

using namespace clang;

RiskScorer::RiskScorer() {}

int RiskScorer::score(const ConversionContext& context) const {
    // Start with a base score from the conversion kind
    int score = 10; // minimum baseline

    // The conversion kind and types would be passed from the visitor;
    // here we use context-based scoring primarily.
    // We'll compute the base risk in the visitor and add context weights here.

    // --- Context-based risk weights ---

    switch (context.type) {
        case ConversionContextType::ARRAY_INDEX:
            score += 30;
            break;

        case ConversionContextType::LOOP_BOUND:
            score += 25;
            break;

        case ConversionContextType::LOOP_INIT:
            score += 15;
            break;

        case ConversionContextType::COMPARISON_OPERAND:
            score += 20;
            if (context.is_sign_mismatch) {
                score += 15; // extra penalty for sign mismatch
            }
            break;

        case ConversionContextType::FUNCTION_ARGUMENT:
            score += 20;
            if (context.is_api_boundary) {
                score += 10; // API boundary is higher risk
            }
            break;

        case ConversionContextType::RETURN_VALUE:
            score += 15;
            break;

        case ConversionContextType::ASSIGNMENT_RHS:
            score += 10;
            break;

        case ConversionContextType::ARITHMETIC_OPERAND:
            score += 15;
            break;

        case ConversionContextType::SWITCH_CONDITION:
            score += 15;
            break;

        case ConversionContextType::IF_CONDITION:
            score += 10;
            break;

        case ConversionContextType::CAST_EXPRESSION:
            score -= 20; // explicit cast already present
            break;

        case ConversionContextType::LOGGING:
            score += 2;
            break;

        case ConversionContextType::SIZEOF_ARGUMENT:
            score += 1; // very low risk
            break;

        case ConversionContextType::PARENTHESIS:
            score += 5;
            break;

        case ConversionContextType::UNKNOWN:
        default:
            score += 5;
            break;
    }

    // --- Modifiers ---

    // Inside explicit cast: developer already acknowledged
    if (context.inside_explicit_cast) {
        score -= 40;
    }

    // Literal source: lower risk (constant, predictable)
    if (context.is_literal) {
        score -= 10;
    }

    // API boundary: higher risk
    if (context.is_api_boundary) {
        score += 10;
    }

    // Deep nesting: harder to reason about
    if (context.nesting_depth > 3) {
        score += 5;
    }

    // Clamp to 0-100
    if (score < 0) score = 0;
    if (score > 100) score = 100;

    return score;
}

RiskLevel RiskScorer::levelFromScore(int score) {
    if (score >= 75) return RiskLevel::CRITICAL;
    if (score >= 50) return RiskLevel::HIGH;
    if (score >= 30) return RiskLevel::MEDIUM;
    return RiskLevel::LOW;
}

std::string RiskScorer::levelLabel(RiskLevel level) {
    switch (level) {
        case RiskLevel::CRITICAL: return "CRITICAL";
        case RiskLevel::HIGH:     return "HIGH";
        case RiskLevel::MEDIUM:   return "MEDIUM";
        case RiskLevel::LOW:      return "LOW";
    }
    return "UNKNOWN";
}

std::string RiskScorer::explain(const ConversionContext& context, int score) const {
    std::ostringstream oss;

    oss << levelLabel(levelFromScore(score)) << " (" << score << "/100): ";

    switch (context.type) {
        case ConversionContextType::ARRAY_INDEX:
            oss << "narrowing/conversion in array index may cause out-of-bounds access";
            break;
        case ConversionContextType::LOOP_BOUND:
            oss << "conversion in loop bound may cause incorrect iteration count";
            break;
        case ConversionContextType::LOOP_INIT:
            oss << "conversion in loop initializer";
            break;
        case ConversionContextType::COMPARISON_OPERAND:
            if (context.is_sign_mismatch) {
                oss << "signed/unsigned comparison mismatch — classic bug pattern";
            } else {
                oss << "implicit conversion in comparison operand";
            }
            break;
        case ConversionContextType::FUNCTION_ARGUMENT:
            oss << "implicit conversion in function argument";
            if (context.is_api_boundary) {
                oss << " at API boundary";
            }
            break;
        case ConversionContextType::RETURN_VALUE:
            oss << "implicit conversion in return value";
            break;
        case ConversionContextType::ASSIGNMENT_RHS:
            oss << "narrowing conversion in assignment";
            break;
        case ConversionContextType::ARITHMETIC_OPERAND:
            oss << "implicit conversion in arithmetic operand may cause truncation";
            break;
        case ConversionContextType::SWITCH_CONDITION:
            oss << "enum-to-integer conversion in switch may hide missing cases";
            break;
        case ConversionContextType::IF_CONDITION:
            oss << "implicit conversion in condition";
            break;
        case ConversionContextType::CAST_EXPRESSION:
            oss << "conversion inside explicit cast (developer acknowledged)";
            break;
        case ConversionContextType::LOGGING:
            oss << "conversion in logging/printing (low risk)";
            break;
        case ConversionContextType::SIZEOF_ARGUMENT:
            oss << "conversion in sizeof argument (very low risk)";
            break;
        case ConversionContextType::PARENTHESIS:
            oss << "conversion in parenthesis expression";
            break;
        default:
            oss << "implicit conversion in unknown context";
            break;
    }

    if (!context.loop_variable.empty()) {
        oss << " [loop variable: " << context.loop_variable << "]";
    }
    if (!context.function_name.empty()) {
        oss << " [function: " << context.function_name << "]";
    }
    if (context.nesting_depth > 3) {
        oss << " [deeply nested: depth=" << context.nesting_depth << "]";
    }

    return oss.str();
}

int RiskScorer::baseRiskForConversionKind(const std::string& kind,
                                            const std::string& source_type,
                                            const std::string& target_type) const {
    int base = 10;

    // Float/double to integer: truncation risk
    if (kind == "FloatingToIntegral" || kind == "FloatingReal") {
        base = 40;
    }

    // Signed to unsigned (or vice versa): sign flip risk
    if (kind == "IntegralCast" || kind == "IntegralToBoolean" ||
        kind == "IntegralToFloating" || kind == "IntegralToPointer" ||
        kind == "PointerToIntegral") {

        // Check for sign change
        bool source_signed = source_type.find("signed") != std::string::npos ||
                             (source_type.find("int") != std::string::npos &&
                              source_type.find("unsigned") == std::string::npos);
        bool target_unsigned = target_type.find("unsigned") != std::string::npos;

        if ((source_signed && target_unsigned) ||
            (!source_signed && target_unsigned)) {
            base = 35;
        }
    }

    // Pointer to integer or integer to pointer: portability risk
    if (kind == "PointerToIntegral" || kind == "IntegralToPointer") {
        base = 30;
    }

    // Larger to smaller integer: narrowing
    // (simplified: we'd need bitwidth comparison for precision)
    if (kind == "IntegralCast") {
        auto getTypeWidth = [](const std::string& t) -> int {
            if (t.find("long long") != std::string::npos ||
                t.find("int64") != std::string::npos) return 64;
            if (t.find("long") != std::string::npos ||
                t.find("int32") != std::string::npos) return 32;
            if (t.find("short") != std::string::npos ||
                t.find("int16") != std::string::npos) return 16;
            if (t.find("char") != std::string::npos ||
                t.find("int8") != std::string::npos) return 8;
            if (t.find("int") != std::string::npos) return 32;
            return 32; // default
        };
        int src_width = getTypeWidth(source_type);
        int tgt_width = getTypeWidth(target_type);
        if (src_width > tgt_width) {
            base = 25; // narrowing
        }
    }

    return base;
}

int RiskScorer::contextWeight(ConversionContextType type) const {
    switch (type) {
        case ConversionContextType::ARRAY_INDEX:        return 30;
        case ConversionContextType::LOOP_BOUND:         return 25;
        case ConversionContextType::LOOP_INIT:          return 15;
        case ConversionContextType::COMPARISON_OPERAND: return 20;
        case ConversionContextType::FUNCTION_ARGUMENT:  return 20;
        case ConversionContextType::RETURN_VALUE:       return 15;
        case ConversionContextType::ASSIGNMENT_RHS:     return 10;
        case ConversionContextType::ARITHMETIC_OPERAND: return 15;
        case ConversionContextType::SWITCH_CONDITION:   return 15;
        case ConversionContextType::IF_CONDITION:       return 10;
        case ConversionContextType::CAST_EXPRESSION:    return -20;
        case ConversionContextType::LOGGING:            return 2;
        case ConversionContextType::SIZEOF_ARGUMENT:    return 1;
        case ConversionContextType::PARENTHESIS:        return 5;
        default:                                        return 5;
    }
}
