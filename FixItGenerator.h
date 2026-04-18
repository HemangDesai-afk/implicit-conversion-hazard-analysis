#ifndef FIXIT_GENERATOR_H
#define FIXIT_GENERATOR_H

#include "RiskScorer.h"
#include <string>

/// Generates fix-it suggestions for high-risk implicit conversions
class FixItGenerator {
public:
    FixItGenerator();

    /// Generate a human-readable fix suggestion for a finding
    std::string generateFix(const Finding& finding) const;

    /// Generate a machine-applicable replacement string (for -fix mode)
    std::string generateReplacement(const Finding& finding) const;

    /// Get a one-line description of the fix category
    std::string fixCategory(const Finding& finding) const;

private:
    /// Suggest an explicit static_cast
    std::string suggestStaticCast(const Finding& finding) const;

    /// Suggest changing the variable/parameter type
    std::string suggestTypeChange(const Finding& finding) const;

    /// Suggest using a wider type for the source expression
    std::string suggestWiderType(const Finding& finding) const;

    /// For sign-mismatch comparisons, suggest the correct approach
    std::string suggestSignFix(const Finding& finding) const;

    /// For narrowing in loop bounds, suggest the pattern fix
    std::string suggestLoopFix(const Finding& finding) const;
};

#endif // FIXIT_GENERATOR_H
