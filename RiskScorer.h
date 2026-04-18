#ifndef RISK_SCORER_H
#define RISK_SCORER_H

#include "ContextCollector.h"
#include <string>

/// Risk severity levels
enum class RiskLevel {
    LOW,       // 0-29: Generally safe, likely intentional
    MEDIUM,    // 30-49: Worth reviewing
    HIGH,      // 50-74: Likely dangerous, should fix
    CRITICAL   // 75-100: Almost certainly a bug
};

/// Represents a scored implicit conversion finding
struct Finding {
    std::string file;
    int line;
    int column;
    std::string source_type;
    std::string target_type;
    std::string conversion_kind;
    int risk_score;
    RiskLevel risk_level;
    ConversionContext context;
    std::string explanation;
    std::string fix_suggestion;
};

/// Computes risk scores for implicit conversions based on context
class RiskScorer {
public:
    RiskScorer();

    /// Score a conversion and return the numeric risk (0-100)
    int score(const ConversionContext& context) const;

    /// Get the human-readable risk level for a score
    static RiskLevel levelFromScore(int score);

    /// Get a human-readable label for a risk level
    static std::string levelLabel(RiskLevel level);

    /// Generate an explanation for why this conversion is risky
    std::string explain(const ConversionContext& context, int score) const;

    /// Base risk score for the conversion kind itself (public for visitor use)
    int baseRiskForConversionKind(const std::string& kind,
                                   const std::string& source_type,
                                   const std::string& target_type) const;

private:
    /// Context weight multipliers
    int contextWeight(ConversionContextType type) const;
};

#endif // RISK_SCORER_H
