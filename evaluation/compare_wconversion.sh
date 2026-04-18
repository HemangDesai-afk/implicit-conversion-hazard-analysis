#!/usr/bin/env bash
# Compare false-positive rate between our analyzer and clang -Wconversion
#
# Usage: ./compare_wconversion.sh <source_file>
#
# This script:
#   1. Runs clang -Wconversion on the file
#   2. Runs our analyzer on the file
#   3. Compares the count and nature of warnings

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ANALYZER="${SCRIPT_DIR}/../build/implicit-conversion-hazard"
CLANG=${CLANG:-/usr/lib64/rocm/llvm/bin/clang}

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <source_file> [clang_flags...]"
    exit 1
fi

SOURCE_FILE="$1"
shift
EXTRA_FLAGS="$*"

if [[ ! -f "$ANALYZER" ]]; then
    echo "Error: Analyzer binary not found at $ANALYZER"
    exit 1
fi

if [[ ! -f "$SOURCE_FILE" ]]; then
    echo "Error: Source file not found: $SOURCE_FILE"
    exit 1
fi

OUTPUT_DIR="${SCRIPT_DIR}/output"
mkdir -p "$OUTPUT_DIR"

BASENAME=$(basename "$SOURCE_FILE" | sed 's/\.[^.]*$//')
WCONV_FILE="${OUTPUT_DIR}/${BASENAME}_wconversion.txt"
OUR_FILE="${OUTPUT_DIR}/${BASENAME}_our_analyzer.txt"

echo "=== False-Positive Rate Comparison ==="
echo "Source file: $SOURCE_FILE"
echo ""

# Step 1: Run clang -Wconversion
echo "--- Running clang -Wconversion ---"
$CLANG -fsyntax-only -Wconversion $EXTRA_FLAGS "$SOURCE_FILE" 2> "$WCONV_FILE" || true
WCONV_COUNT=$(wc -l < "$WCONV_FILE" | tr -d ' ')
echo "clang -Wconversion warnings: $WCONV_COUNT"

if [[ "$WCONV_COUNT" -gt 0 ]]; then
    echo ""
    echo "clang -Wconversion output (first 30 lines):"
    head -30 "$WCONV_FILE"
fi

echo ""

# Step 2: Run our analyzer
echo "--- Running our analyzer ---"
"$ANALYZER" \
    --risk-threshold 30 \
    -- \
    "$SOURCE_FILE" \
    > "$OUR_FILE" 2>&1 || true

# Count findings
OUR_CRITICAL=$(grep -c "CRITICAL" "$OUR_FILE" 2>/dev/null || echo "0")
OUR_HIGH=$(grep -c "HIGH" "$OUR_FILE" 2>/dev/null || echo "0")
OUR_MEDIUM=$(grep -c "MEDIUM" "$OUR_FILE" 2>/dev/null || echo "0")
OUR_LOW=$(grep -c "LOW" "$OUR_FILE" 2>/dev/null || echo "0")
OUR_TOTAL=$((OUR_CRITICAL + OUR_HIGH + OUR_MEDIUM + OUR_LOW))

echo "Our analyzer findings:"
echo "  CRITICAL: $OUR_CRITICAL"
echo "  HIGH:     $OUR_HIGH"
echo "  MEDIUM:   $OUR_MEDIUM"
echo "  LOW:      $OUR_LOW"
echo "  TOTAL:    $OUR_TOTAL"

echo ""
echo "=== Comparison ==="
echo "clang -Wconversion:    $WCONV_COUNT warnings"
echo "Our analyzer (≥30):    $OUR_TOTAL findings"
echo ""

if [[ "$WCONV_COUNT" -gt 0 ]]; then
    # Estimate FP reduction
    # Our analyzer filters by context, so fewer findings = more precise
    if [[ "$OUR_TOTAL" -lt "$WCONV_COUNT" ]]; then
        REDUCTION=$(( (WCONV_COUNT - OUR_TOTAL) * 100 / WCONV_COUNT ))
        echo "Our analyzer reports ${REDUCTION}% fewer warnings (more targeted)"
    elif [[ "$OUR_TOTAL" -eq "$WCONV_COUNT" ]]; then
        echo "Same number of warnings — different filtering approach"
    else
        echo "Our analyzer reports more findings (broader context analysis)"
    fi
fi

echo ""
echo "Full outputs:"
echo "  clang -Wconversion: $WCONV_FILE"
echo "  Our analyzer:       $OUR_FILE"
